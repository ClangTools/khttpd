#include "websocket_client.hpp"
#include <iostream>
#include <boost/url.hpp>

namespace khttpd::framework::client
{
  WebsocketClient::WebsocketClient(net::io_context& ioc)
    : ws_(net::make_strand(ioc)), resolver_(ioc)
  {
  }

  void WebsocketClient::connect(const std::string& url, ConnectCallback callback)
  {
    connect_callback_ = std::move(callback);

    // Parse URL
    auto url_result = boost::urls::parse_uri(url);
    if (!url_result.has_value())
    {
      if (connect_callback_) connect_callback_(beast::error_code(beast::http::error::bad_target));
      return;
    }
    auto u = url_result.value();
    host_ = u.host();
    std::string port = u.port();
    if (port.empty()) port = "80"; // Default for WS

    resolver_.async_resolve(host_, port,
                            beast::bind_front_handler(&WebsocketClient::on_resolve, shared_from_this()));
  }

  void WebsocketClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
  {
    if (ec)
    {
      if (connect_callback_) connect_callback_(ec);
      return;
    }

    beast::get_lowest_layer(ws_).async_connect(results,
                                               beast::bind_front_handler(
                                                 &WebsocketClient::on_connect, shared_from_this()));
  }

  void WebsocketClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
  {
    if (ec)
    {
      if (connect_callback_) connect_callback_(ec);
      return;
    }

    // Set suggested timeout settings for the websocket
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws_.async_handshake(host_, "/",
                        beast::bind_front_handler(&WebsocketClient::on_handshake, shared_from_this()));
  }

  void WebsocketClient::on_handshake(beast::error_code ec)
  {
    if (ec)
    {
      if (connect_callback_) connect_callback_(ec);
      return;
    }

    if (connect_callback_) connect_callback_(ec);
    do_read();
  }

  void WebsocketClient::send(const std::string& message)
  {
    ws_.async_write(net::buffer(message),
                    beast::bind_front_handler(&WebsocketClient::on_write, shared_from_this()));
  }

  void WebsocketClient::on_write(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);
    if (ec)
    {
      if (on_error_) on_error_(ec);
    }
  }

  void WebsocketClient::do_read()
  {
    ws_.async_read(buffer_,
                   beast::bind_front_handler(&WebsocketClient::on_read, shared_from_this()));
  }

  void WebsocketClient::on_read(beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);
    if (ec)
    {
      if (on_error_) on_error_(ec);
      if (on_close_) on_close_();
      return;
    }

    std::string msg = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    if (on_message_) on_message_(msg);

    do_read();
  }

  void WebsocketClient::close()
  {
    ws_.async_close(websocket::close_code::normal,
                    beast::bind_front_handler(&WebsocketClient::on_close, shared_from_this()));
  }

  void WebsocketClient::on_close(beast::error_code ec)
  {
    if (ec)
    {
      if (on_error_) on_error_(ec);
    }
    if (on_close_) on_close_();
  }

  void WebsocketClient::set_on_message(MessageHandler handler) { on_message_ = std::move(handler); }
  void WebsocketClient::set_on_error(ErrorHandler handler) { on_error_ = std::move(handler); }
  void WebsocketClient::set_on_close(CloseHandler handler) { on_close_ = std::move(handler); }
}
