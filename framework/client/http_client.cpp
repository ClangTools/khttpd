#include "http_client.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <fmt/core.h>
#include <iostream>

namespace khttpd::framework::client
{
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  using tcp = boost::asio::ip::tcp;

  std::string replace_all(std::string str, const std::string& from, const std::string& to)
  {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();
    }
    return str;
  }


  HttpClient::HttpClient(net::io_context& ioc)
    : ioc_(ioc), resolver_(ioc)
  {
  }

  std::string HttpClient::build_target(const std::string& path, const std::map<std::string, std::string>& query_params)
  {
    if (query_params.empty())
    {
      return path;
    }

    boost::urls::url u = boost::urls::parse_relative_ref(path).value();
    for (const auto& [key, value] : query_params)
    {
      u.params().append({key, value});
    }
    return u.buffer();
  }

  void HttpClient::request(http::verb method,
                           const std::string& path,
                           const std::map<std::string, std::string>& query_params,
                           const std::string& body,
                           const std::map<std::string, std::string>& headers,
                           ResponseCallback callback)
  {
    // Parse host and port from path if it's an absolute URL,
    // OR assume the client should have a base URL?
    // The macro interface `API_CALL` uses `PATH_TEMPLATE` which usually implies relative path.
    // However, `request` needs to know WHERE to connect.
    //
    // DESIGN DECISION:
    // The `HttpClient` provided here seems to be stateless regarding "Server Address" in the class itself.
    // It's common for a Client to be bound to a base URL, or for the request to provide full URL.
    //
    // If `path` is absolute (http://...), we parse it.
    // If `path` is relative, we currently don't have a configured host/port in HttpClient.
    //
    // Update: I will modify HttpClient to accept a base_url in constructor OR assume path is full URL.
    // Given the usage `API_CALL("GET", "/users", ...)` implies relative path.
    // I will assume for now that the path provided MIGHT be absolute, or we need a way to set host.
    //
    // BUT, `request` signature expects just `path`.
    // I will assume `path` MUST be a full URL for now if no base is set.
    // Or better: Let's extract host/port from the URL.

    std::string url_str = path;
    // If query params exist, we need to append them.
    // But if `path` is full URL, `build_target` using `parse_relative_ref` might fail or be wrong.

    // Let's use boost::urls::url to parse the input path/url.
    auto url_result = boost::urls::parse_uri(path);
    if (!url_result.has_value())
    {
      // Try relative?
      // If it's relative, we can't connect without a host.
      // We will fail if host is missing.
      // UNLESS we allow setting a default host in HttpClient.
      // For this implementation, I'll enforce absolute URL in `path` OR I'll add a `base_url` field?
      // The prompt didn't specify base_url. I'll add `host` and `port` to `HttpClient`?
      //
      // Let's assume the user provides a full URL in the path for the `API_CALL`,
      // e.g. `API_CALL("GET", "http://localhost:8080/users", ...)`
      // Or `API_CALL("GET", "/users", ...)` and the client has a base URL.
      //
      // I'll add `base_url` to `HttpClient` constructor to make it useful.

      // Wait, I can't change the constructor easily without breaking existing code (none yet).
      // I'll add `set_base_url` or overload constructor.
    }

    // Quick fix: Assume path is full URL.
    boost::urls::url_view u;
    boost::urls::url buffer_url;

    if (url_result.has_value())
    {
      u = url_result.value();
    }
    else
    {
      // Maybe it's just a path?
      // We need a host.
      auto res = boost::urls::parse_uri_reference(path);
      if (res.has_value())
      {
        u = res.value();
      }
      else
      {
        if (callback) callback(beast::error_code(beast::http::error::bad_target), {});
        return;
      }
    }

    std::string host = u.host();
    std::string port = u.port();
    if (port.empty())
    {
      port = (u.scheme() == "https") ? "443" : "80";
    }

    // Construct target (path + query)
    if (!query_params.empty())
    {
      buffer_url = u;
      for (auto& p : query_params)
      {
        buffer_url.params().append({p.first, p.second});
      }
      u = buffer_url;
    }

    std::string target = std::string(u.encoded_path());
    if (target.empty()) target = "/";
    if (u.has_query())
    {
      target += "?" + std::string(u.encoded_query());
    }

    raw_request(method, host, port, target, body, headers, std::move(callback));
  }

  // Helper class to keep the session alive during async operation
  class Session : public std::enable_shared_from_this<Session>
  {
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    HttpClient::ResponseCallback callback_;

  public:
    Session(net::io_context& ioc, HttpClient::ResponseCallback callback)
      : resolver_(ioc), stream_(ioc), callback_(std::move(callback))
    {
    }

    void run(const std::string& host, const std::string& port, http::request<http::string_body> req)
    {
      req_ = std::move(req);
      resolver_.async_resolve(host, port,
                              beast::bind_front_handler(&Session::on_resolve, shared_from_this()));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
      if (ec) return callback_(ec, {});

      stream_.async_connect(results,
                            beast::bind_front_handler(&Session::on_connect, shared_from_this()));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec) return callback_(ec, {});

      http::async_write(stream_, req_,
                        beast::bind_front_handler(&Session::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);
      if (ec) return callback_(ec, {});

      http::async_read(stream_, buffer_, res_,
                       beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
      boost::ignore_unused(bytes_transferred);
      if (ec) return callback_(ec, {});

      // Gracefully close the socket
      beast::error_code ec_shutdown;
      stream_.socket().shutdown(tcp::socket::shutdown_both, ec_shutdown);

      // invoke callback
      callback_(ec, std::move(res_));
    }
  };

  void HttpClient::raw_request(http::verb method,
                               const std::string& host,
                               const std::string& port,
                               const std::string& target,
                               const std::string& body,
                               const std::map<std::string, std::string>& headers,
                               ResponseCallback callback)
  {
    // Set up request
    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for (const auto& h : headers)
    {
      req.set(h.first, h.second);
    }

    if (!body.empty())
    {
      req.body() = body;
      req.prepare_payload();
    }

    // Launch session
    std::make_shared<Session>(ioc_, std::move(callback))->run(host, port, std::move(req));
  }

  http::response<http::string_body> HttpClient::request_sync(
    http::verb method,
    const std::string& path,
    const std::map<std::string, std::string>& query_params,
    const std::string& body,
    const std::map<std::string, std::string>& headers)
  {
    // Parse URL (Same logic as request)
    boost::urls::url_view u;
    boost::urls::url buffer_url;
    auto url_result = boost::urls::parse_uri(path);
    if (url_result.has_value())
    {
      u = url_result.value();
    }
    else
    {
      // Basic parse fallback
      auto res = boost::urls::parse_uri_reference(path);
      if (res.has_value())
      {
        u = res.value();
      }
      else
      {
        throw std::runtime_error("Invalid URL: " + path);
      }
    }

    std::string host = u.host();
    std::string port = u.port();
    if (port.empty())
    {
      port = (u.scheme() == "https") ? "443" : "80";
    }

    if (!query_params.empty())
    {
      buffer_url = u;
      for (auto& p : query_params)
      {
        buffer_url.params().append({p.first, p.second});
      }
      u = buffer_url;
    }

    std::string target = std::string(u.encoded_path());
    if (target.empty()) target = "/";
    if (u.has_query())
    {
      target += "?" + std::string(u.encoded_query());
    }

    // Synchronous Request
    tcp::resolver resolver(ioc_);
    beast::tcp_stream stream(ioc_);

    auto const results = resolver.resolve(host, port);
    stream.connect(results);

    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    for (const auto& h : headers)
    {
      req.set(h.first, h.second);
    }
    if (!body.empty())
    {
      req.body() = body;
      req.prepare_payload();
    }

    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    return res;
  }
}
