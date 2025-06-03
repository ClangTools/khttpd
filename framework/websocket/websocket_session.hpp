// framework/websocket/websocket_session.hpp
#ifndef KHTTPD_FRAMEWORK_WEBSOCKET_SESSION_HPP
#define KHTTPD_FRAMEWORK_WEBSOCKET_SESSION_HPP

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

#include "router/websocket_router.hpp" // 需要 WebsocketRouter 的完整定义

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace khttpd::framework
{
  class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
  {
  public:
    WebsocketSession(tcp::socket&& socket, WebsocketRouter& ws_router, const std::string& initial_path);
    virtual ~WebsocketSession() = default;

    template <class Body, class Allocator>
    void run_handshake(http::request<Body, http::basic_fields<Allocator>> req);

    virtual void send_message(const std::string& msg, bool is_text);

  private:
    ws::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    WebsocketRouter& websocket_router_;
    std::string initial_path_;

    void on_handshake(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void do_write(std::shared_ptr<const std::string> ss, bool is_text);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void do_close(beast::error_code ec = {});
  };

  template <class Body, class Allocator>
  void WebsocketSession::run_handshake(http::request<Body, http::basic_fields<Allocator>> req)
  {
    ws_.async_accept(req,
                     beast::bind_front_handler(&WebsocketSession::on_handshake, shared_from_this()));
  }
}
#endif // KHTTPD_FRAMEWORK_WEBSOCKET_SESSION_HPP