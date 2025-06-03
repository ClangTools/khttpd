// framework/server.cpp
#include "server.hpp"
#include "session/http_session.hpp" // 需要HttpSession
#include <fmt/core.h>


namespace khttpd::framework
{
  Server::Server(const tcp::endpoint& endpoint, int num_threads)
    : ioc_(std::in_place, num_threads),
      num_threads_(num_threads),
      signals_(*ioc_, SIGINT, SIGTERM),
      acceptor_(net::make_strand(*ioc_))
  {
    boost::beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
      fmt::print(stderr, "Server open error: {}\n", ec.message());
      throw std::runtime_error(fmt::format("Failed to open acceptor: {}", ec.message()));
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec)
    {
      fmt::print(stderr, "Server set_option reuse_address error: {}\n", ec.message());
      throw std::runtime_error(fmt::format("Failed to set reuse_address: {}", ec.message()));
    }

    acceptor_.bind(endpoint, ec);
    if (ec)
    {
      fmt::print(stderr, "Server bind error: {}\n", ec.message());
      throw std::runtime_error(fmt::format("Failed to bind acceptor: {}", ec.message()));
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec)
    {
      fmt::print(stderr, "Server listen error: {}\n", ec.message());
      throw std::runtime_error(fmt::format("Failed to listen: {}", ec.message()));
    }

  }

  HttpRouter& Server::get_http_router()
  {
    return http_router_;
  }

  const HttpRouter& Server::get_http_router() const
  {
    return http_router_;
  }

  WebsocketRouter& Server::get_websocket_router()
  {
    return websocket_router_;
  }

  const WebsocketRouter& Server::get_websocket_router() const
  {
    return websocket_router_;
  }

  void Server::run()
  {
    fmt::print("Server listening on {}:{}\n", acceptor_.local_endpoint().address().to_string(),
               acceptor_.local_endpoint().port());

    signals_.async_wait(beast::bind_front_handler(&Server::handle_signal, shared_from_this()));

    do_accept();

    threads_.reserve(num_threads_ - 1);
    for (int i = 0; i < num_threads_ - 1; ++i)
    {
      threads_.emplace_back([&ioc = *ioc_]
      {
        ioc.run();
      });
    }

    (*ioc_).run();

    for (auto& t : threads_)
    {
      t.join();
    }
    fmt::print("Server workers stopped.\n");
  }

  void Server::stop()
  {
    boost::beast::error_code ec;
    acceptor_.close(ec);
    if (ec)
    {
      fmt::print(stderr, "Server acceptor close error: {}\n", ec.message());
    }

    if (ioc_.has_value())
    {
      (*ioc_).stop();
    }
    fmt::print("Server stopped.\n");
  }

  void Server::do_accept()
  {
    acceptor_.async_accept(
      net::make_strand(*ioc_),
      beast::bind_front_handler(&Server::on_accept, shared_from_this()));
  }

  void Server::on_accept(boost::beast::error_code ec, tcp::socket socket)
  {
    if (ec)
    {
      if (ec != boost::system::errc::operation_canceled)
      {
        fmt::print(stderr, "Server on_accept error: {}\n", ec.message());
      }
    }
    else
    {
      std::make_shared<HttpSession>(std::move(socket), http_router_, websocket_router_)->run();
    }

    if (acceptor_.is_open())
    {
      do_accept();
    }
  }

  void Server::handle_signal(const boost::beast::error_code& error, int signal_number)
  {
    if (!error)
    {
      fmt::print("Received signal {}, shutting down gracefully...\n", signal_number);
      stop();
    }
  }
} // namespace khttpd::framework
