// framework/router/websocket_router.cpp
#include "websocket_router.hpp"
#include <fmt/core.h>

namespace khttpd::framework
{
  WebsocketRouter::WebsocketRouter() = default;

  void WebsocketRouter::add_handler(const std::string& path,
                                    WebsocketOpenHandler on_open,
                                    WebsocketMessageHandler on_message,
                                    WebsocketCloseHandler on_close,
                                    WebsocketErrorHandler on_error)
  {
    WebsocketRouteEntry entry;
    entry.on_open = std::move(on_open);
    entry.on_message = std::move(on_message);
    entry.on_close = std::move(on_close);
    entry.on_error = std::move(on_error);
    handlers_[path] = entry;
    fmt::print("Registered WebSocket handlers for path: {}\n", path);
  }

  void WebsocketRouter::dispatch_open(const std::string& path, WebsocketContext& ctx)
  {
    auto it = handlers_.find(path);
    if (it != handlers_.end() && it->second.on_open)
    {
      it->second.on_open(ctx);
    }
    else
    {
      fmt::print(stderr, "No on_open handler found for WS path: {}\n", path);
    }
  }

  void WebsocketRouter::dispatch_message(const std::string& path, WebsocketContext& ctx)
  {
    auto it = handlers_.find(path);
    if (it != handlers_.end() && it->second.on_message)
    {
      it->second.on_message(ctx);
    }
    else
    {
      fmt::print(stderr, "No on_message handler found for WS path: {}\n", path);
      // Default behavior: if no handler, just close connection? Echo?
      // For now, nothing happens.
    }
  }

  void WebsocketRouter::dispatch_close(const std::string& path, WebsocketContext& ctx)
  {
    auto it = handlers_.find(path);
    if (it != handlers_.end() && it->second.on_close)
    {
      it->second.on_close(ctx);
    }
    else
    {
      fmt::print(stderr, "No on_close handler found for WS path: {}\n", path);
    }
  }

  void WebsocketRouter::dispatch_error(const std::string& path, WebsocketContext& ctx)
  {
    auto it = handlers_.find(path);
    if (it != handlers_.end() && it->second.on_error)
    {
      it->second.on_error(ctx);
    }
    else
    {
      fmt::print(stderr, "No on_error handler found for WS path: {} (error: {})\n", path, ctx.error_code.message());
    }
  }
}
