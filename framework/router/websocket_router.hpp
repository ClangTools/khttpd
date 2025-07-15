// framework/router/websocket_router.hpp
#ifndef KHTTPD_FRAMEWORK_ROUTER_WEBSOCKET_ROUTER_HPP
#define KHTTPD_FRAMEWORK_ROUTER_WEBSOCKET_ROUTER_HPP

#include <functional>
#include <string>
#include <map>
#include "context/websocket_context.hpp"

namespace khttpd::framework
{
  namespace websocket
  {
    static bool send_message(const std::string& id, const std::string& msg, bool is_text);
    static size_t send_message(const std::vector<std::string>& ids, const std::string& msg, bool is_text);
  }

  using WebsocketOpenHandler = std::function<void(WebsocketContext&)>;
  using WebsocketMessageHandler = std::function<void(WebsocketContext&)>;
  using WebsocketCloseHandler = std::function<void(WebsocketContext&)>;
  using WebsocketErrorHandler = std::function<void(WebsocketContext&)>;

  // 路由条目，包含所有事件的处理器
  struct WebsocketRouteEntry
  {
    WebsocketOpenHandler on_open;
    WebsocketMessageHandler on_message;
    WebsocketCloseHandler on_close;
    WebsocketErrorHandler on_error;
  };

  class WebsocketRouter
  {
  public:
    WebsocketRouter();

    void add_handler(const std::string& path,
                     WebsocketOpenHandler on_open = {nullptr},
                     WebsocketMessageHandler on_message = {nullptr},
                     WebsocketCloseHandler on_close = {nullptr},
                     WebsocketErrorHandler on_error = {nullptr});

    void dispatch_open(const std::string& path, WebsocketContext& ctx);
    void dispatch_message(const std::string& path, WebsocketContext& ctx);
    void dispatch_close(const std::string& path, WebsocketContext& ctx);
    void dispatch_error(const std::string& path, WebsocketContext& ctx);

  private:
    std::map<std::string, WebsocketRouteEntry> handlers_; // 路径 -> 处理器集合
  };
}
#endif // KHTTPD_FRAMEWORK_ROUTER_WEBSOCKET_ROUTER_HPP
