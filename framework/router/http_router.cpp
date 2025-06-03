// framework/router/http_router.cpp
#include "http_router.hpp"
#include <fmt/core.h>

namespace khttpd::framework
{
  HttpRouter::HttpRouter()
  {
  }

  std::tuple<std::regex, std::vector<std::string>, int, int> HttpRouter::parse_path_pattern(
    const std::string& path_pattern)
  {
    std::string regex_str = "^";
    std::vector<std::string> param_names;
    std::regex param_regex(":([a-zA-Z_][a-zA-Z0-9_]*)"); // search :paramName

    int literal_segments = 0;
    int dynamic_segments = 0;

    std::sregex_iterator it(path_pattern.begin(), path_pattern.end(), param_regex);
    std::sregex_iterator end;

    std::string::const_iterator current_pos = path_pattern.begin();
    int param_count = 0;

    for (std::sregex_iterator temp_it(path_pattern.begin(), path_pattern.end(), param_regex); temp_it != end; ++temp_it)
    {
      param_count++;
    }

    int current_param_index = 0;

    it = std::sregex_iterator(path_pattern.begin(), path_pattern.end(), param_regex);

    while (it != end)
    {
      std::string literal_part = std::string(current_pos, it->prefix().second);
      if (!literal_part.empty())
      {
        size_t start = 0;
        size_t found = literal_part.find('/');
        while (found != std::string::npos)
        {
          if (found > start)
          {
            literal_segments++;
          }
          start = found + 1;
          found = literal_part.find('/', start);
        }
        if (literal_part.length() > start)
        {
          literal_segments++;
        }
      }
      regex_str += std::regex_replace(literal_part, std::regex(R"([\.\+\*\?\|\(\)\[\]\{\}\^\$])"), "\\$&");

      param_names.push_back(it->str().substr(1));
      dynamic_segments++;

      if (current_param_index == param_count - 1)
      {
        regex_str += "(.*)";
      }
      else
      {
        regex_str += "([^/]+)";
      }
      current_param_index++;

      current_pos = it->suffix().first;
      ++it;
    }
    std::string tail_literal_part = std::string(current_pos, path_pattern.end());
    if (!tail_literal_part.empty())
    {
      size_t start = 0;
      size_t found = tail_literal_part.find('/');
      while (found != std::string::npos)
      {
        if (found > start)
        {
          literal_segments++;
        }
        start = found + 1;
        found = tail_literal_part.find('/', start);
      }
      if (tail_literal_part.length() > start)
      {
        literal_segments++;
      }
    }
    regex_str += std::regex_replace(tail_literal_part, std::regex(R"([\.\+\*\?\|\(\)\[\]\{\}\^\$])"), "\\$&");
    regex_str += "$";

    return {std::regex(regex_str), param_names, literal_segments, dynamic_segments};
  }

  void HttpRouter::add_route(const std::string& path_pattern, boost::beast::http::verb method, HttpHandler handler)
  {
    for (auto& entry : routes_)
    {
      if (entry.original_path == path_pattern)
      {
        entry.handlers[method] = std::move(handler);
        fmt::print("Updated handler for route: {} {}\n", boost::beast::http::to_string(method), path_pattern);
        return;
      }
    }

    RouteEntry new_entry;
    new_entry.original_path = path_pattern;
    auto [regex, params, literal_count, dynamic_count] = parse_path_pattern(path_pattern);
    new_entry.path_regex = std::move(regex);
    new_entry.param_names = std::move(params);
    new_entry.literal_segments_count = literal_count;
    new_entry.dynamic_segments_count = dynamic_count;
    new_entry.handlers[method] = std::move(handler);

    routes_.push_back(std::move(new_entry));
    std::sort(routes_.begin(), routes_.end(), RouteEntry::compare_specificity);
    fmt::print("Registered dynamic route: {} {} (literal:{}, dynamic:{})\n",
               boost::beast::http::to_string(method), path_pattern, literal_count, dynamic_count);
  }

  void HttpRouter::get(const std::string& path, HttpHandler handler)
  {
    add_route(path, boost::beast::http::verb::get, std::move(handler));
  }

  void HttpRouter::post(const std::string& path, HttpHandler handler)
  {
    add_route(path, boost::beast::http::verb::post, std::move(handler));
  }

  void HttpRouter::put(const std::string& path, HttpHandler handler)
  {
    add_route(path, boost::beast::http::verb::put, std::move(handler));
  }

  void HttpRouter::del(const std::string& path, HttpHandler handler)
  {
    add_route(path, boost::beast::http::verb::delete_, std::move(handler));
  }

  void HttpRouter::options(const std::string& path, HttpHandler handler)
  {
    add_route(path, boost::beast::http::verb::options, std::move(handler));
  }

  void HttpRouter::dispatch(HttpContext& ctx)
  {
    std::string request_path = ctx.path();
    boost::beast::http::verb request_method = ctx.method();

    for (const auto& entry : routes_)
    {
      std::smatch matches;
      if (std::regex_match(request_path, matches, entry.path_regex))
      {
        auto method_it = entry.handlers.find(request_method);
        if (method_it != entry.handlers.end())
        {
          std::map<std::string, std::string> path_params;
          for (size_t i = 0; i < entry.param_names.size(); ++i)
          {
            if (i + 1 < matches.size())
            {
              path_params[entry.param_names[i]] = matches[i + 1].str();
            }
          }
          ctx.set_path_params(std::move(path_params));

          method_it->second(ctx);
          return;
        }
        else
        {
          handle_method_not_allowed(ctx, entry.handlers); // 传递允许的方法映射
          return;
        }
      }
    }

    handle_not_found(ctx);
  }

  void HttpRouter::handle_not_found(HttpContext& ctx)
  {
    ctx.set_status(boost::beast::http::status::not_found);
    ctx.set_content_type("text/html");
    ctx.set_body(fmt::format("<h1>404 Not Found</h1><p>The resource '{}' was not found on this server.</p>",
                             ctx.path()));
    fmt::print(stderr, "404 Not Found: {}\n", ctx.path());
  }

  void HttpRouter::handle_method_not_allowed(HttpContext& ctx,
                                             const std::map<boost::beast::http::verb, HttpHandler>& allowed_methods)
  {
    ctx.set_status(boost::beast::http::status::method_not_allowed);
    ctx.set_content_type("text/html");
    ctx.set_body(fmt::format("<h1>405 Method Not Allowed</h1><p>Method {} not allowed for resource '{}'.</p>",
                             boost::beast::http::to_string(ctx.method()), ctx.path()));

    std::string allowed_methods_str;
    bool first = true;
    for (const auto& pair : allowed_methods)
    {
      if (!first) allowed_methods_str += ", ";
      allowed_methods_str += boost::beast::http::to_string(pair.first);
      first = false;
    }
    ctx.set_header(boost::beast::http::field::allow, allowed_methods_str);
    fmt::print(stderr, "405 Method Not Allowed: {} {}\n", boost::beast::http::to_string(ctx.method()), ctx.path());
  }
}
