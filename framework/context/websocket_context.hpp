#ifndef KHTTPD_FRAMEWORK_WEBSOCKET_CONTEXT_HPP
#define KHTTPD_FRAMEWORK_WEBSOCKET_CONTEXT_HPP

#include <string>
#include <memory>
#include <boost/beast/core/error.hpp>

namespace khttpd
{
  namespace framework
  {
    class WebsocketSession;
  }
}

namespace khttpd::framework
{
  class WebsocketContext
  {
  public:
    std::weak_ptr<WebsocketSession> session_weak_ptr;
    std::string message;
    bool is_text;
    boost::beast::error_code error_code;
    std::string path;
    std::weak_ptr<WebsocketSession> session_;

    WebsocketContext(std::weak_ptr<WebsocketSession> session, std::string  msg, bool text,
                     std::string  path);
    WebsocketContext(std::weak_ptr<WebsocketSession> session, std::string  path,
                     boost::beast::error_code ec = {});

    void send(const std::string& msg, bool is_text = true);
  };
}
#endif // KHTTPD_FRAMEWORK_WEBSOCKET_CONTEXT_HPP
