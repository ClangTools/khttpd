#ifndef KHTTPD_FRAMEWORK_CLIENT_WEBSOCKET_CLIENT_HPP
#define KHTTPD_FRAMEWORK_CLIENT_WEBSOCKET_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace khttpd::framework::client
{
  namespace beast = boost::beast;
  namespace websocket = beast::websocket;
  namespace net = boost::asio;
  using tcp = boost::asio::ip::tcp;

  class WebsocketClient : public std::enable_shared_from_this<WebsocketClient>
  {
  public:
    using ConnectCallback = std::function<void(beast::error_code)>;
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(beast::error_code)>;
    using CloseHandler = std::function<void()>;

    explicit WebsocketClient(net::io_context& ioc);

    void connect(const std::string& url, ConnectCallback callback);
    void send(const std::string& message);
    void close();

    void set_on_message(MessageHandler handler);
    void set_on_error(ErrorHandler handler);
    void set_on_close(CloseHandler handler);

  private:
    websocket::stream<beast::tcp_stream> ws_;
    tcp::resolver resolver_;
    std::string host_;
    beast::flat_buffer buffer_;

    ConnectCallback connect_callback_;
    MessageHandler on_message_;
    ErrorHandler on_error_;
    CloseHandler on_close_;

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);

    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void on_close(beast::error_code ec);
  };
}

#endif // KHTTPD_FRAMEWORK_CLIENT_WEBSOCKET_CLIENT_HPP
