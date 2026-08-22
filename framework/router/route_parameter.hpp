#ifndef KHTTPD_FRAMEWORK_ROUTER_ROUTE_PARAMETER_HPP_
#define KHTTPD_FRAMEWORK_ROUTER_ROUTE_PARAMETER_HPP_

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace khttpd::framework
{
  enum class RouteParameterLocation
  {
    path,
    query,
  };

  struct RouteParameterDocumentation
  {
    std::string name;
    RouteParameterLocation location;
    bool required;
    boost::json::value schema;
  };

  template <class T>
  struct PathParam
  {
    using value_type = T;

    explicit PathParam(std::string parameter_name) : name(std::move(parameter_name)) {}

    std::string name;
  };

  template <class T>
  struct QueryParam
  {
    using value_type = T;

    explicit QueryParam(std::string parameter_name) : name(std::move(parameter_name)) {}
    QueryParam(std::string parameter_name, T default_parameter_value)
      : name(std::move(parameter_name)), default_value(std::move(default_parameter_value)) {}

    std::string name;
    std::optional<T> default_value;
  };

  template <class T>
  struct Body
  {
    using value_type = T;
  };
}

namespace khttpd::framework::detail
{
  template <class T>
  struct is_route_parameter_descriptor : std::false_type {};

  template <class T>
  struct is_route_parameter_descriptor<PathParam<T>> : std::true_type {};

  template <class T>
  struct is_route_parameter_descriptor<QueryParam<T>> : std::true_type {};

  template <class T>
  struct is_route_parameter_descriptor<Body<T>> : std::true_type {};

  template <class T>
  inline constexpr bool is_route_parameter_descriptor_v =
    is_route_parameter_descriptor<std::decay_t<T>>::value;

  template <class T>
  struct is_body_descriptor : std::false_type {};

  template <class T>
  struct is_body_descriptor<Body<T>> : std::true_type {};

  template <class T>
  inline constexpr bool is_body_descriptor_v = is_body_descriptor<std::decay_t<T>>::value;
}

#endif
