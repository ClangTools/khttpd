// framework/websocket/websocket_session.cpp
#include "websocket_session.hpp"
#include "context/websocket_context.hpp"
#include <fmt/core.h>

namespace khttpd::framework
{
  WebsocketSession::WebsocketSession(tcp::socket&& socket, WebsocketRouter& ws_router,
                                     const std::string& initial_path)
    : ws_(std::move(socket)),
      websocket_router_(ws_router),
      initial_path_(initial_path)
  {
    ws_.set_option(ws::stream_base::decorator([](ws::response_type& res)
    {
      res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " khttpd-websocket");
    }));
  }

  void WebsocketSession::on_handshake(beast::error_code ec)
  {
    if (ec)
    {
      fmt::print(stderr, "WebSocket handshake error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }
    fmt::print("WebSocket handshake successful for path: {}\n", initial_path_);

    WebsocketContext open_ctx(shared_from_this(), initial_path_);
    websocket_router_.dispatch_open(initial_path_, open_ctx);

    do_read();
  }

  void WebsocketSession::do_read()
  {
    ws_.async_read(buffer_,
                   beast::bind_front_handler(&WebsocketSession::on_read, shared_from_this()));
  }

  void WebsocketSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec == ws::error::closed)
    {
      fmt::print("WebSocket connection for path '{}' closed by client.\n", initial_path_);
      do_close(ec);
      return;
    }
    if (ec)
    {
      fmt::print(stderr, "WebSocket read error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }

    std::string received_message = beast::buffers_to_string(buffer_.data());
    bool is_text = ws_.got_text();

    fmt::print("Received WS message on path '{}': {}\n", initial_path_, received_message);

    buffer_.consume(buffer_.size());

    WebsocketContext message_ctx(shared_from_this(), received_message, is_text, initial_path_);
    websocket_router_.dispatch_message(initial_path_, message_ctx);

    do_read();
  }

  void WebsocketSession::send_message(const std::string& msg, bool is_text_msg)
  {
    auto ss = std::make_shared<const std::string>(msg);
    do_write(ss, is_text_msg);
  }

  void WebsocketSession::do_write(std::shared_ptr<const std::string> ss, bool is_text_msg)
  {
    ws_.text(is_text_msg);
    ws_.async_write(net::buffer(*ss),
                    beast::bind_front_handler(&WebsocketSession::on_write, shared_from_this()));
  }

  void WebsocketSession::on_write(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
      fmt::print(stderr, "WebSocket write error for path '{}': {}\n", initial_path_, ec.message());
      do_close(ec);
      return;
    }
  }

  void WebsocketSession::do_close(beast::error_code ec)
  {
    if (ec && ec != ws::error::closed && ec != boost::asio::error::eof)
    {
      WebsocketContext error_ctx(shared_from_this(), initial_path_, ec);
      websocket_router_.dispatch_error(initial_path_, error_ctx);
    }
    else
    {
      WebsocketContext close_ctx(shared_from_this(), initial_path_, ec);
      websocket_router_.dispatch_close(initial_path_, close_ctx);
    }
  }
}
