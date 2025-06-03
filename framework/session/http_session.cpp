#include "http_session.hpp"
#include "context/http_context.hpp"

#include <fmt/core.h>


using namespace khttpd::framework;

HttpSession::HttpSession(tcp::socket&& socket, HttpRouter& router, WebsocketRouter& ws_router)
  : stream_(std::move(socket)),
    router_(router),
    websocket_router_(ws_router)
{
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
