#ifndef KHTTPD_FRAMEWORK_CLIENT_HTTP_CLIENT_HPP
#define KHTTPD_FRAMEWORK_CLIENT_HTTP_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/url.hpp>
#include <boost/json.hpp>
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <future>
#include <type_traits>

namespace khttpd::framework::client
{
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace net = boost::asio;
  using tcp = boost::asio::ip::tcp;

  // Helper functions for macros
  std::string replace_all(std::string str, const std::string& from, const std::string& to);

  // String conversion helper
  template <typename T>
  std::string to_string(const T& val)
  {
    if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char*>)
    {
      return std::string(val);
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
      return std::to_string(val);
    }
    else
    {
      // Fallback: try using ostream operator if available, or just throw/error
      // For now, assume it's something serialize-able or simple.
      // Let's assume user passes simple types for query/path params.
      return std::to_string(val);
    }
  }

  // Specialize for string explicitly to avoid ambiguity if needed
  inline std::string to_string(const std::string& val) { return val; }
  inline std::string to_string(const char* val) { return std::string(val); }

  template <typename T>
  std::string serialize_body(const T& value)
  {
    if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char*>)
    {
      return std::string(value);
    }
    else if constexpr (std::is_same_v<T, boost::json::value> || std::is_same_v<T, boost::json::object> || std::is_same_v
      <T, boost::json::array>)
    {
      return boost::json::serialize(value);
    }
    else
    {
      // Try to serialize using boost::json::value_from
      return boost::json::serialize(boost::json::value_from(value));
    }
  }

  class HttpClient : public std::enable_shared_from_this<HttpClient>
  {
  public:
    using ResponseCallback = std::function<void(beast::error_code, http::response<http::string_body>)>;

    explicit HttpClient(net::io_context& ioc);

    // Async Request
    void request(http::verb method,
                 const std::string& path,
                 const std::map<std::string, std::string>& query_params,
                 const std::string& body,
                 const std::map<std::string, std::string>& headers,
                 ResponseCallback callback);

    // Sync Request
    http::response<http::string_body> request_sync(
      http::verb method,
      const std::string& path,
      const std::map<std::string, std::string>& query_params,
      const std::string& body,
      const std::map<std::string, std::string>& headers);

    // Raw request helper (if user wants to construct everything manually)
    void raw_request(http::verb method,
                     const std::string& host,
                     const std::string& port,
                     const std::string& target,
                     const std::string& body,
                     const std::map<std::string, std::string>& headers,
                     ResponseCallback callback);

  private:
    net::io_context& ioc_;
    tcp::resolver resolver_;

    // Helper to construct URL with query params
    static std::string build_target(const std::string& path, const std::map<std::string, std::string>& query_params);
  };
}

// Include macros at the end so they see the namespace and class
#include "macros.hpp"

#endif // KHTTPD_FRAMEWORK_CLIENT_HTTP_CLIENT_HPP
