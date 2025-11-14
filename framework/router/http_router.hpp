// framework/router/http_router.hpp
#ifndef KHTTPD_FRAMEWORK_ROUTER_HTTP_ROUT
#define KHTTPD_FRAMEWORK_ROUTER_HTTP_ROUT

#include "context/http_context.hpp"
#include <functional>
#include <string>
#include <map>
#include <vector>
#include <regex>

namespace khttpd::framework
{
  using HttpHandler = std::function<void(HttpContext&)>;

  // 路由条目结构
  struct RouteEntry
  {
    std::string original_path;
    std::regex path_regex;
    std::vector<std::string> param_names;
    std::map<boost::beast::http::verb, HttpHandler> handlers;
    int literal_segments_count = 0;
    int dynamic_segments_count = 0;

    // 比较函数：用于根据特异性对路由进行排序
    // 返回 true 表示 a 应该排在 b 之前 (a 更具体)
    static bool compare_specificity(const RouteEntry& a, const RouteEntry& b)
    {
      // 首先比较字面路径段数量，多的优先
      if (a.literal_segments_count != b.literal_segments_count)
      {
        return a.literal_segments_count > b.literal_segments_count;
      }
      // 如果字面路径段数量相同，则比较动态路径段数量，少的优先
      return a.dynamic_segments_count < b.dynamic_segments_count;
    }
  };

  class HttpRouter
  {
  public:
    HttpRouter();

    void get(const std::string& path, HttpHandler handler);
    void post(const std::string& path, HttpHandler handler);
    void put(const std::string& path, HttpHandler handler);
    void del(const std::string& path, HttpHandler handler);
    void options(const std::string& path, HttpHandler handler);

    bool dispatch(HttpContext& ctx) const;

  private:
    std::vector<RouteEntry> routes_;

    void add_route(const std::string& path_pattern, boost::beast::http::verb method, HttpHandler handler);

    static std::tuple<std::regex, std::vector<std::string>, int, int> parse_path_pattern(
      const std::string& path_pattern);

    static void handle_not_found(HttpContext& ctx);
    static void handle_method_not_allowed(HttpContext& ctx,
                                          const std::map<boost::beast::http::verb, HttpHandler>& allowed_methods);
  };
}
#endif // KHTTPD_FRAMEWORK_ROUTER_HTTP_ROUT
