// framework/controller/http_controller.hpp
#ifndef KHTTPD_FRAMEWORK_CONTROLLER_HTTP_CONTROLLER_HPP
#define KHTTPD_FRAMEWORK_CONTROLLER_HTTP_CONTROLLER_HPP
#include <fmt/format.h>

#include "context/http_context.hpp"
#include "router/http_router.hpp"


namespace khttpd::framework
{
#ifndef KHTTPD_ROUTE
#define KHTTPD_ROUTE(VERB, PATH, METHOD_NAME) \
router.VERB(PATH, bind_handler(&std::decay_t<decltype(*this)>::METHOD_NAME))
#endif
#ifndef KHTTPD_WSROUTE
#define KHTTPD_WSROUTE_NULL_HANDLER nullptr

#define KHTTPD_WSROUTE_BIND_HANDLER(METHOD_NAME) \
    bind_handler(&std::decay_t<decltype(*this)>::METHOD_NAME)

#define KHTTPD_WSROUTE_2(PATH, ONMESSAGE_METHOD_NAME) \
    router.add_handler(PATH, \
        KHTTPD_WSROUTE_NULL_HANDLER, /* ONOPEN */ \
        KHTTPD_WSROUTE_BIND_HANDLER(ONMESSAGE_METHOD_NAME), \
        KHTTPD_WSROUTE_NULL_HANDLER, /* ONCLOSE */ \
        KHTTPD_WSROUTE_NULL_HANDLER  /* ONERROR */ \
    )

#define KHTTPD_WSROUTE_3(PATH, ONMESSAGE_METHOD_NAME, ONCLOSE_METHOD_NAME) \
    router.add_handler(PATH, \
        KHTTPD_WSROUTE_NULL_HANDLER, /* ONOPEN */ \
        KHTTPD_WSROUTE_BIND_HANDLER(ONMESSAGE_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONCLOSE_METHOD_NAME), \
        KHTTPD_WSROUTE_NULL_HANDLER  /* ONERROR */ \
    )

#define KHTTPD_WSROUTE_4(PATH, ONOPEN_METHOD_NAME, ONMESSAGE_METHOD_NAME, ONCLOSE_METHOD_NAME) \
    router.add_handler(PATH, \
        KHTTPD_WSROUTE_BIND_HANDLER(ONOPEN_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONMESSAGE_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONCLOSE_METHOD_NAME), \
        KHTTPD_WSROUTE_NULL_HANDLER  /* ONERROR */ \
    )

#define KHTTPD_WSROUTE_5(PATH, ONOPEN_METHOD_NAME, ONMESSAGE_METHOD_NAME, ONCLOSE_METHOD_NAME, ONERROR_METHOD_NAME) \
    router.add_handler(PATH, \
        KHTTPD_WSROUTE_BIND_HANDLER(ONOPEN_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONMESSAGE_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONCLOSE_METHOD_NAME), \
        KHTTPD_WSROUTE_BIND_HANDLER(ONERROR_METHOD_NAME) \
    )

#define KHTTPD_WSROUTE_GET_MACRO(_1, _2, _3, _4, _5, N, ...) N

#define KHTTPD_WSROUTE(...) \
    KHTTPD_WSROUTE_GET_MACRO(__VA_ARGS__, KHTTPD_WSROUTE_5, KHTTPD_WSROUTE_4, KHTTPD_WSROUTE_3, KHTTPD_WSROUTE_2)(__VA_ARGS__)

#endif


  template <typename Derived>
  class BaseController : public std::enable_shared_from_this<Derived>
  {
  public:
    virtual ~BaseController() = default;

    virtual std::shared_ptr<BaseController> register_routes(HttpRouter& router) = 0;

    inline virtual std::shared_ptr<BaseController> register_routes(WebsocketRouter& router)
    {
      return this->shared_from_this();
    }

  protected:
    template <typename MethodPtr>
    inline auto bind_handler(MethodPtr method_ptr)
    {
      return std::bind(method_ptr, this->shared_from_this(), std::placeholders::_1);
    }
  };
} // namespace khttpd::framework

#endif // KHTTPD_FRAMEWORK_CONTROLLER_HTTP_CONTROLLER_HPP
