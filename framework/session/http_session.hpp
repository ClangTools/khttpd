#ifndef KHTTPD_HTTP_SESSION_HPP
#define KHTTPD_HTTP_SESSION_HPP

#include <boost/beast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include "router/http_router.hpp"
#include "websocket/websocket_session.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace khttpd::framework
{
  class HttpRouter;
  class WebsocketRouter;
  class WebsocketSession;

  // 处理单个HTTP连接的会话
  class HttpSession : public std::enable_shared_from_this<HttpSession>
  {
  public:
    HttpSession(tcp::socket&& socket, HttpRouter& router, WebsocketRouter& ws_router);

    // 启动会话
    void run();

  private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    HttpRouter& router_;
    WebsocketRouter& websocket_router_;
    std::shared_ptr<WebsocketSession> ws_session_;

    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    void handle_request();

    void send_response(http::message_generator msg);
    void on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred);

    void do_close();

    void handle_websocket_upgrade();
  };
}
#endif // KHTTPD_HTTP_SESSION_HPP