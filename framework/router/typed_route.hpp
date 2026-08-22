#ifndef KHTTPD_FRAMEWORK_ROUTER_TYPED_ROUTE_HPP_
#define KHTTPD_FRAMEWORK_ROUTER_TYPED_ROUTE_HPP_

#include "router/http_result.hpp"
#include "router/openapi_schema.hpp"
#include "router/route_parameter.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cctype>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>

namespace khttpd::framework
{
  // Thrown when a typed route cannot parse a request body before invoking its handler.
  class TypedRequestValidationError final : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  class TypedParameterValidationError final : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };
}

namespace khttpd::framework::detail
{
  struct TypedRouteHandler
  {
    std::function<void(HttpContext&)> handler;
    std::optional<boost::json::value> request_schema;
    std::optional<boost::json::value> response_schema;
    std::vector<RouteParameterDocumentation> parameters;
  };

  template <class T>
  struct typed_response_body
  {
    using type = T;
  };

  template <class T>
  struct typed_response_body<HttpResult<T>>
  {
    using type = T;
  };

  template <class R, class... Args>
  struct callable_signature
  {
    using return_type = R;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t Index>
    using argument = std::tuple_element_t<Index, std::tuple<Args...>>;
  };

  template <class T>
  struct callable_traits : callable_traits<decltype(&std::decay_t<T>::operator())> {};

  template <class R, class... Args>
  struct callable_traits<R(Args...)> : callable_signature<R, Args...> {};

  template <class R, class... Args>
  struct callable_traits<R (*)(Args...)> : callable_signature<R, Args...> {};

  template <class R, class... Args>
  struct callable_traits<std::function<R(Args...)>> : callable_signature<R, Args...> {};

  template <class C, class R, class... Args>
  struct callable_traits<R (C::*)(Args...)> : callable_signature<R, Args...> {};

  template <class C, class R, class... Args>
  struct callable_traits<R (C::*)(Args...) const> : callable_signature<R, Args...> {};

  template <class T>
  using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

  inline void write_invalid_request_body(HttpContext& context)
  {
    context.set_status(boost::beast::http::status::bad_request);
    boost::json::object error;
    error.emplace("code", "INVALID_REQUEST_BODY");
    error.emplace("message", "Request body must be valid JSON matching the expected schema");
    context.set_body_json(error);
  }

  inline void write_invalid_request_parameter(HttpContext& context, const std::string& message)
  {
    context.set_status(boost::beast::http::status::bad_request);
    boost::json::object error;
    error.emplace("code", "INVALID_REQUEST_PARAMETER");
    error.emplace("message", message);
    context.set_body_json(error);
  }

  inline bool is_json_media_type(std::string content_type)
  {
    if (const auto semicolon = content_type.find(';'); semicolon != std::string::npos)
    {
      content_type.resize(semicolon);
    }

    const auto first = std::find_if_not(content_type.begin(), content_type.end(), [](const unsigned char c)
    {
      return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(content_type.rbegin(), content_type.rend(), [](const unsigned char c)
    {
      return std::isspace(c) != 0;
    }).base();
    if (first >= last) return false;

    const auto media_type = ascii_lower(std::string(first, last));
    constexpr std::string_view prefix = "application/";
    constexpr std::string_view suffix = "+json";
    return media_type == "application/json" ||
      (media_type.size() > prefix.size() + suffix.size() &&
       media_type.compare(0, prefix.size(), prefix) == 0 &&
       media_type.compare(media_type.size() - suffix.size(), suffix.size(), suffix) == 0);
  }

  template <class T>
  T parse_parameter_value(const std::string& value, const char* location, const std::string& name)
  {
    using Value = remove_cvref_t<T>;
    if constexpr (is_optional_v<Value>)
    {
      using Item = typename is_optional<Value>::value_type;
      return Value{parse_parameter_value<Item>(value, location, name)};
    }
    else if constexpr (std::is_same_v<Value, std::string>)
    {
      return value;
    }
    else if constexpr (std::is_same_v<Value, bool>)
    {
      if (value == "true") return true;
      if (value == "false") return false;
    }
    else if constexpr (std::is_integral_v<Value>)
    {
      Value converted{};
      const auto result = std::from_chars(value.data(), value.data() + value.size(), converted);
      if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) return converted;
    }
    else if constexpr (std::is_floating_point_v<Value>)
    {
      Value converted{};
      const auto result = std::from_chars(value.data(), value.data() + value.size(), converted);
      if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) return converted;
    }
    else
    {
      static_assert(std::is_same_v<Value, void>,
                    "route parameters support string, bool, integral, and floating-point values");
    }

    throw TypedParameterValidationError(
      std::string("Invalid ") + location + " parameter '" + name + "'");
  }

  template <class T>
  T read_route_parameter(const PathParam<T>& descriptor, HttpContext& context)
  {
    const auto value = context.get_path_param(descriptor.name);
    if (!value)
      throw TypedParameterValidationError("Missing path parameter '" + descriptor.name + "'");
    return parse_parameter_value<T>(*value, "path", descriptor.name);
  }

  template <class T>
  T read_route_parameter(const QueryParam<T>& descriptor, HttpContext& context)
  {
    const auto value = context.get_query_param(descriptor.name);
    if (value) return parse_parameter_value<T>(*value, "query", descriptor.name);
    if (descriptor.default_value) return *descriptor.default_value;
    if constexpr (is_optional_v<T>) return std::nullopt;
    throw TypedParameterValidationError("Missing query parameter '" + descriptor.name + "'");
  }

  template <class T>
  T read_route_parameter(const Body<T>&, HttpContext& context)
  {
    const auto content_type = context.get_header(boost::beast::http::field::content_type);
    if (!content_type || !is_json_media_type(*content_type))
      throw TypedRequestValidationError("Request body must be valid JSON matching the expected schema");

    try
    {
      return boost::json::value_to<T>(boost::json::parse(context.body()));
    }
    catch (const std::bad_alloc&)
    {
      throw;
    }
    catch (const std::exception&)
    {
      throw TypedRequestValidationError("Request body must be valid JSON matching the expected schema");
    }
  }

  template <class Descriptor>
  void set_request_schema(std::optional<boost::json::value>& schema)
  {
    using Value = std::decay_t<Descriptor>;
    if constexpr (std::is_same_v<Value, Body<typename Value::value_type>>)
      schema.emplace(openapi_schema<typename Value::value_type>());
  }

  template <class T>
  void append_parameter_documentation(std::vector<RouteParameterDocumentation>& parameters,
                                      const PathParam<T>& descriptor)
  {
    parameters.push_back(
      {descriptor.name, RouteParameterLocation::path, true, openapi_schema<T>()});
  }

  template <class T>
  void append_parameter_documentation(std::vector<RouteParameterDocumentation>& parameters,
                                      const QueryParam<T>& descriptor)
  {
    auto schema = openapi_schema<T>();
    if (descriptor.default_value)
    {
      if constexpr (is_optional_v<T>)
      {
        if (*descriptor.default_value)
          schema.as_object().emplace("default", boost::json::value_from(**descriptor.default_value));
      }
      else
      {
        schema.as_object().emplace("default", boost::json::value_from(*descriptor.default_value));
      }
    }
    parameters.push_back({descriptor.name, RouteParameterLocation::query,
                          !descriptor.default_value && !is_optional_v<T>, std::move(schema)});
  }

  template <class T>
  void append_parameter_documentation(std::vector<RouteParameterDocumentation>&, const Body<T>&)
  {
  }

  template <class Response, class Handler, class... Descriptors>
  TypedRouteHandler make_parameterized_typed_handler_with_response(
    Handler&& input_handler, Descriptors&&... input_descriptors)
  {
    static_assert((is_route_parameter_descriptor_v<Descriptors> && ...),
                  "all typed route bindings must be route parameter descriptors");
    static_assert((0U + ... + (is_body_descriptor_v<Descriptors> ? 1U : 0U)) <= 1U,
                  "a typed route can register at most one Body descriptor");
    using StoredHandler = std::decay_t<Handler>;
    static_assert(!std::is_void_v<Response>,
                  "typed handlers must return a body or HttpResult<void>");
    static_assert(!std::is_reference_v<Response>,
                  "typed handlers must return responses by value");

    std::vector<RouteParameterDocumentation> parameters;
    (append_parameter_documentation(parameters, input_descriptors), ...);
    auto descriptors = std::make_tuple(std::forward<Descriptors>(input_descriptors)...);
    auto adapted = [handler = StoredHandler(std::forward<Handler>(input_handler)),
                    descriptors = std::move(descriptors)](HttpContext& context) mutable
    {
      auto values = std::apply([&context](const auto&... descriptor)
      {
        return std::make_tuple(read_route_parameter(descriptor, context)...);
      }, descriptors);

      std::apply([&](auto&... value)
      {
        if constexpr (std::is_invocable_v<StoredHandler&, decltype(value)..., HttpContext&>)
        {
          auto response = std::invoke(handler, value..., context);
          apply_typed_response(context, std::move(response));
        }
        else
        {
          static_assert(std::is_invocable_v<StoredHandler&, decltype(value)...>,
                        "typed handler arguments must match the registered route descriptors");
          auto response = std::invoke(handler, value...);
          apply_typed_response(context, std::move(response));
        }
      }, values);
    };

    std::optional<boost::json::value> request_schema;
    (set_request_schema<std::decay_t<Descriptors>>(request_schema), ...);
    using ResponseBody = typename typed_response_body<Response>::type;
    std::optional<boost::json::value> response_schema;
    if constexpr (!std::is_void_v<ResponseBody>) response_schema.emplace(openapi_schema<ResponseBody>());
    return {std::move(adapted), std::move(request_schema), std::move(response_schema),
            std::move(parameters)};
  }

  template <class Handler, class... Descriptors>
  TypedRouteHandler make_parameterized_typed_handler(Handler&& input_handler,
                                                      Descriptors&&... input_descriptors)
  {
    using Response = typename callable_traits<std::decay_t<Handler>>::return_type;
    return make_parameterized_typed_handler_with_response<Response>(
      std::forward<Handler>(input_handler), std::forward<Descriptors>(input_descriptors)...);
  }

  template <class Controller, class Response, class... Arguments, class... Descriptors>
  TypedRouteHandler make_parameterized_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(Arguments...),
    Descriptors&&... descriptors)
  {
    auto bound = [controller = std::move(controller), method](auto&... value) -> Response
    {
      return std::invoke(method, *controller, value...);
    };
    return make_parameterized_typed_handler_with_response<Response>(
      std::move(bound), std::forward<Descriptors>(descriptors)...);
  }

  template <class Controller, class Response, class... Arguments, class... Descriptors>
  TypedRouteHandler make_parameterized_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(Arguments...) const,
    Descriptors&&... descriptors)
  {
    auto bound = [controller = std::move(controller), method](auto&... value) -> Response
    {
      return std::invoke(method, *controller, value...);
    };
    return make_parameterized_typed_handler_with_response<Response>(
      std::move(bound), std::forward<Descriptors>(descriptors)...);
  }

  template <class Handler>
  TypedRouteHandler make_typed_handler(Handler&& input_handler)
  {
    using StoredHandler = std::decay_t<Handler>;
    using Traits = callable_traits<StoredHandler>;
    static_assert(Traits::arity == 1 || Traits::arity == 2,
                  "typed handlers must accept (const Request&) or (const Request&, HttpContext&)");

    using RequestArgument = typename Traits::template argument<0>;
    using Request = remove_cvref_t<RequestArgument>;
    using Response = typename Traits::return_type;

    static_assert(!std::is_same_v<Request, HttpContext>,
                  "legacy HttpContext handlers must use the existing route overload");
    static_assert(!std::is_void_v<Response>,
                  "typed handlers must return a body or HttpResult<void>");
    static_assert(!std::is_reference_v<Response>,
                  "typed handlers must return responses by value");

    if constexpr (Traits::arity == 2)
    {
      using ContextArgument = typename Traits::template argument<1>;
      static_assert(std::is_same_v<ContextArgument, HttpContext&>,
                    "the optional second typed-handler argument must be HttpContext&");
    }

    auto adapted = [handler = StoredHandler(std::forward<Handler>(input_handler))](HttpContext& context) mutable
    {
      const auto content_type = context.get_header(boost::beast::http::field::content_type);
      if (!content_type || !is_json_media_type(*content_type))
      {
        throw TypedRequestValidationError(
          "Request body must be valid JSON matching the expected schema");
      }

      std::optional<boost::json::value> json;
      std::optional<Request> request;
      try
      {
        json.emplace(boost::json::parse(context.body()));
        request.emplace(boost::json::value_to<Request>(*json));
      }
      catch (const std::bad_alloc&)
      {
        throw;
      }
      catch (const std::exception&)
      {
        throw TypedRequestValidationError(
          "Request body must be valid JSON matching the expected schema");
      }

      if constexpr (Traits::arity == 1)
      {
        auto response = std::invoke(handler, static_cast<const Request&>(*request));
        apply_typed_response(context, std::move(response));
      }
      else
      {
        auto response = std::invoke(handler, static_cast<const Request&>(*request), context);
        apply_typed_response(context, std::move(response));
      }
    };

    using ResponseBody = typename typed_response_body<Response>::type;
    std::optional<boost::json::value> response_schema;
    if constexpr (!std::is_void_v<ResponseBody>) response_schema.emplace(openapi_schema<ResponseBody>());
    return {std::move(adapted), openapi_schema<Request>(), std::move(response_schema), {}};
  }

  template <class Controller, class Response, class Request>
  TypedRouteHandler make_typed_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(const Request&))
  {
    std::function<Response(const Request&)> bound =
      [controller = std::move(controller), method](const Request& request)
      {
        return std::invoke(method, *controller, request);
      };
    return make_typed_handler(std::move(bound));
  }

  template <class Controller, class Response, class Request>
  TypedRouteHandler make_typed_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(const Request&) const)
  {
    std::function<Response(const Request&)> bound =
      [controller = std::move(controller), method](const Request& request)
      {
        return std::invoke(method, *controller, request);
      };
    return make_typed_handler(std::move(bound));
  }

  template <class Controller, class Response, class Request>
  TypedRouteHandler make_typed_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(const Request&, HttpContext&))
  {
    std::function<Response(const Request&, HttpContext&)> bound =
      [controller = std::move(controller), method](const Request& request, HttpContext& context)
      {
        return std::invoke(method, *controller, request, context);
      };
    return make_typed_handler(std::move(bound));
  }

  template <class Controller, class Response, class Request>
  TypedRouteHandler make_typed_member_handler(
    std::shared_ptr<Controller> controller,
    Response (Controller::*method)(const Request&, HttpContext&) const)
  {
    std::function<Response(const Request&, HttpContext&)> bound =
      [controller = std::move(controller), method](const Request& request, HttpContext& context)
      {
        return std::invoke(method, *controller, request, context);
      };
    return make_typed_handler(std::move(bound));
  }
}

#endif // KHTTPD_FRAMEWORK_ROUTER_TYPED_ROUTE_HPP_
