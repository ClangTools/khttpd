#include "http_session.hpp"
#include "context/http_context.hpp"
#include <fmt/core.h>
#include <utility>


using namespace khttpd::framework;

// 辅助函数：根据文件扩展名获取 MIME 类型
std::string HttpSession::mime_type_from_extension(const std::string& ext)
{
  if (ext == ".html" || ext == ".htm") return "text/html";
  if (ext == ".css") return "text/css";
  if (ext == ".js") return "application/javascript";
  if (ext == ".json") return "application/json";
  if (ext == ".png") return "image/png";
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".gif") return "image/gif";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".pdf") return "application/pdf";
  if (ext == ".txt") return "text/plain";
  // Add more MIME types as needed
  return "application/octet-stream"; // Default for unknown types
}

HttpSession::HttpSession(tcp::socket&& socket, HttpRouter& router, WebsocketRouter& ws_router,
                         const std::string& web_root)
  : stream_(std::move(socket)),
    router_(router),
    websocket_router_(ws_router),
    web_root_path_(web_root)
{
  boost::system::error_code ec;
  // 在构造函数中规范化 web_root 路径，避免重复操作
  canonical_web_root_path_ = boost::filesystem::canonical(web_root_path_, ec);
  if (ec)
  {
    fmt::print(stderr, "Error canonicalizing web_root_path_ '{}': {}\n", web_root_path_.string(), ec.message());
    // 如果 web_root 本身就无效，后续静态文件服务都会失败
    disable_web_root_ = true;
  }
}

void HttpSession::run()
{
  net::dispatch(stream_.get_executor(),
                beast::bind_front_handler(&HttpSession::do_read, shared_from_this()));
}

void HttpSession::do_read()
{
  req_ = {};
  http::async_read(stream_, buffer_, req_,
                   beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
  boost::ignore_unused(bytes_transferred);

  if (ec == http::error::end_of_stream)
  {
    return do_close();
  }
  if (ec)
  {
    fmt::print(stderr, "HttpSession on_read error: {}\n", ec.message());
    return;
  }

  if (boost::beast::websocket::is_upgrade(req_))
  {
    fmt::print("Detected WebSocket upgrade request for target: {}\n", req_.target());
    handle_websocket_upgrade();
    return;
  }

  handle_request();
}

void HttpSession::handle_request()
{
  // 处理 GET 请求以尝试服务静态文件
  if (req_.method() == http::verb::get || req_.method() == http::verb::head)
  {
    if (do_serve_static_file())
    {
      return; // 静态文件已处理，无需进一步路由
    }
  }

  res_ = {};

  HttpContext ctx(req_, res_);

  router_.dispatch(ctx);

  send_response(std::move(res_));
}

void HttpSession::send_response(http::message_generator msg)
{
  bool keep_alive = msg.keep_alive();
  beast::async_write(stream_, std::move(msg),
                     beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), keep_alive));
}

void HttpSession::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred)
{
  boost::ignore_unused(bytes_transferred);

  if (ec)
  {
    fmt::print(stderr, "HttpSession on_write error: {}\n", ec.message());
    return;
  }

  if (!keep_alive)
  {
    return do_close();
  }

  do_read();
}

void HttpSession::do_close()
{
  beast::error_code ec;
  stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
  if (ec)
  {
    fmt::print(stderr, "HttpSession shutdown error: {}\n", ec.message());
  }
}

void HttpSession::handle_websocket_upgrade()
{
  ws_session_ = std::make_shared<WebsocketSession>(stream_.release_socket(), websocket_router_,
                                                   std::string(req_.target()));

  ws_session_->run_handshake(req_);
}


// 尝试服务静态文件
bool HttpSession::do_serve_static_file()
{
  // 确保 web_root_path_ 是有效的
  if (canonical_web_root_path_.empty())
  {
    // web_root 本身就无效，不尝试服务静态文件
    return false;
  }

  // 使用 HttpContext 获取请求路径，因为它已经去除了查询字符串
  HttpContext temp_ctx(req_, res_); // 临时创建 ctx 来获取 path()
  boost::filesystem::path request_path = temp_ctx.path();

  // 如果请求的是根路径，尝试提供 index.html
  if (request_path == "/")
  {
    request_path = "/index.html";
  }

  boost::filesystem::path full_local_path = web_root_path_ / request_path.relative_path();
  boost::system::error_code ec;

  // 1. 规范化路径以防止目录遍历攻击 (e.g., /../)
  full_local_path = boost::filesystem::canonical(full_local_path, ec);

  // 如果规范化失败 (文件不存在、权限问题、路径无效等)
  if (ec)
  {
    if (ec == boost::system::errc::no_such_file_or_directory)
    {
      // 文件不存在，交由动态路由或 404 处理
      return false;
    }
    // 其他错误，例如权限不足或无效路径，直接返回 403
    http::response<http::string_body> forbidden_res{http::status::forbidden, req_.version()};
    forbidden_res.keep_alive(req_.keep_alive());
    forbidden_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    forbidden_res.set(http::field::content_type, "text/html");
    forbidden_res.body() = fmt::format("<h1>403 Forbidden</h1><p>Access denied due to invalid path: {}. Error: {}</p>",
                                       request_path.string(), ec.message());
    forbidden_res.prepare_payload();
    send_response(std::move(forbidden_res));
    return true; // 已处理请求
  }

  // 2. 安全检查：确保规范化后的路径仍在 Web 根目录内
  const std::string& full_path_str = full_local_path.string();
  const std::string& root_path_str = canonical_web_root_path_.string();
  if (full_path_str.substr(0, root_path_str.size()) != root_path_str)
  {
    http::response<http::string_body> forbidden_res{http::status::forbidden, req_.version()};
    forbidden_res.keep_alive(req_.keep_alive());
    forbidden_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    forbidden_res.set(http::field::content_type, "text/html");
    forbidden_res.body() = fmt::format(
      "<h1>403 Forbidden</h1><p>Access denied: Path traversal attempt detected for {}.</p>", request_path.string());
    forbidden_res.prepare_payload();
    send_response(std::move(forbidden_res));
    return true; // 已处理请求
  }

  // 3. 检查是否为目录
  if (boost::filesystem::is_directory(full_local_path, ec))
  {
    if (ec)
    {
      /* 错误处理 */
    }
    // 如果是目录，尝试提供 index.html
    boost::filesystem::path index_file_path = full_local_path / "index.html";
    if (boost::filesystem::is_regular_file(index_file_path, ec))
    {
      full_local_path = index_file_path; // 将路径指向 index.html
    }
    else
    {
      // 目录不包含 index.html，且不允许目录列表
      http::response<http::string_body> forbidden_res{http::status::forbidden, req_.version()};
      forbidden_res.keep_alive(req_.keep_alive());
      forbidden_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      forbidden_res.set(http::field::content_type, "text/html");
      forbidden_res.body() = fmt::format("<h1>403 Forbidden</h1><p>Directory listing not allowed for {}.</p>",
                                         request_path.string());
      forbidden_res.prepare_payload();
      send_response(std::move(forbidden_res));
      return true; // 已处理请求
    }
  }

  // 4. 最终检查：确保是常规文件
  if (!boost::filesystem::is_regular_file(full_local_path, ec) || ec)
  {
    // 如果不是常规文件，或者存在其他错误，交由动态路由或 404 处理
    return false;
  }

  // 5. 文件存在且是常规文件，现在开始发送文件
  http::response<http::file_body> file_res;
  file_res.version(req_.version());
  file_res.keep_alive(req_.keep_alive());
  file_res.result(http::status::ok); // 默认 200 OK
  file_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);

  // 打开文件
  file_res.body().open(full_local_path.string().c_str(), beast::file_mode::scan, ec);
  if (ec)
  {
    fmt::print(stderr, "Error opening file {}: {}\n", full_local_path.string(), ec.message());
    http::response<http::string_body> internal_error_res{http::status::internal_server_error, req_.version()};
    internal_error_res.keep_alive(req_.keep_alive());
    internal_error_res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    internal_error_res.set(http::field::content_type, "text/html");
    internal_error_res.body() = "<h1>500 Internal Server Error</h1><p>Could not open the requested file.</p>";
    internal_error_res.prepare_payload();
    send_response(std::move(internal_error_res));
    return true; // 已处理请求
  }

  // 设置 Content-Type
  std::string extension = full_local_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower); // 转换为小写
  file_res.set(http::field::content_type, mime_type_from_extension(extension));

  // 准备 payload (这会自动设置 Content-Length)
  file_res.prepare_payload();

  // 发送响应
  send_response(std::move(file_res));
  return true; // 静态文件已处理
}
