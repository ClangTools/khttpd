// framework/server.hpp
#ifndef KHTTPD_FRAMEWORK_SERVER_HPP
#define KHTTPD_FRAMEWORK_SERVER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/signal_set.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <optional>

// 包含完整定义，因为 Server 现在拥有它们
#include "router/http_router.hpp"
#include "router/websocket_router.hpp"

namespace khttpd::framework
{
  namespace net = boost::asio;
  using tcp = boost::asio::ip::tcp;

  class Server : public std::enable_shared_from_this<Server>
  {
  public:
    // 构造函数：现在只接受端口和线程数量。路由器在内部创建。
    Server(const tcp::endpoint& endpoint, std::string web_root, int num_threads = 1);

    HttpRouter& get_http_router();
    const HttpRouter& get_http_router() const; // const 版本

    void add_interceptor(std::shared_ptr<Interceptor> interceptor);

    WebsocketRouter& get_websocket_router();
    const WebsocketRouter& get_websocket_router() const; // const 版本

    void run();

    void stop();

  private:
    // std::optional<net::io_context> ioc_;
    // int num_threads_;
    std::vector<std::thread> threads_;
    net::signal_set signals_;
    const std::string web_root_;

    tcp::acceptor acceptor_;

    HttpRouter http_router_;
    WebsocketRouter websocket_router_;

    void do_accept();
    void on_accept(boost::beast::error_code ec, tcp::socket socket);
    void handle_signal(const boost::beast::error_code& error, int signal_number);
  };
}
#endif
