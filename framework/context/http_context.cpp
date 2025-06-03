// framework/context/http_context.cpp
#include "http_context.hpp"
#include <fmt/core.h>
#include <algorithm> // for std::remove_if
#include <iomanip>   // for std::quoted (not directly used here, but useful for debugging)
#include <boost/beast/version.hpp>

namespace khttpd::framework
{
  // Helper to trim whitespace from a string
  std::string HttpContext::trim(const std::string& str) const
  {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first)
    {
      return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
  }

  // Helper to extract a header value from a block of headers
  std::string HttpContext::extract_header_value(const std::string& headers, const std::string& header_name) const
  {
    std::string search_str = "\r\n" + header_name + ":";
    size_t pos = headers.find(search_str);
    if (pos == std::string::npos)
    {
      // Also check for start of line for the first header
      if (headers.rfind(header_name + ":", 0) == 0)
      {
        // Check if it starts with header_name:
        pos = 0;
      }
      else
      {
        return "";
      }
    }
    else
    {
      pos += search_str.length();
    }

    size_t end_pos = headers.find("\r\n", pos);
    if (end_pos == std::string::npos)
    {
      end_pos = headers.length();
    }
    return trim(headers.substr(pos, end_pos - pos));
  }


  HttpContext::HttpContext(Request& req, Response& res)
    : req_(req), res_(res)
  {
    res_.version(req_.version());
    res_.keep_alive(req_.keep_alive());
    res_.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res_.set(boost::beast::http::field::content_type, "text/plain");
  }

  void HttpContext::parse_url_components() const
  {
    if (url_parsed_)
    {
      return;
    }
    boost::system::result<boost::urls::url_view> url_result = boost::urls::parse_relative_ref(req_.target());
    if (url_result.has_value())
    {
      parsed_url_ = url_result.value();
      cached_path_ = parsed_url_.path();
    }
    else
    {
      cached_path_ = std::string(req_.target());
      fmt::print(
        stderr,
        "Warning: Failed to parse request target '{}' as relative-ref: {}. Query parameters may not be available.\n",
        req_.target(), url_result.error().message());
    }
    url_parsed_ = true;
  }

  const std::string& HttpContext::path() const
  {
    parse_url_components();
    return cached_path_;
  }

  boost::beast::http::verb HttpContext::method() const
  {
    return req_.method();
  }

  std::string HttpContext::body() const
  {
    return req_.body();
  }

  std::optional<std::string> HttpContext::get_query_param(const std::string& key) const
  {
    parse_url_components();
    for (auto param : parsed_url_.params())
    {
      if (param.key == key)return std::string(param.value);
    }
    return std::nullopt;
  }

  std::optional<std::string> HttpContext::get_path_param(const std::string& key) const
  {
    auto it = path_params_.find(key);
    if (it != path_params_.end())
    {
      return it->second;
    }
    return std::nullopt;
  }

  std::optional<std::string> HttpContext::get_header(boost::beast::string_view name) const
  {
    auto it = req_.find(name);
    if (it != req_.end())
    {
      return std::string(it->value());
    }
    return std::nullopt;
  }

  std::optional<boost::json::value> HttpContext::get_json() const
  {
    if (cached_json_.has_value())
    {
      return cached_json_;
    }
    auto content_type = get_header(boost::beast::http::field::content_type);
    if (!content_type || content_type->find("application/json") == std::string::npos)
    {
      return std::nullopt;
    }
    try
    {
      cached_json_ = boost::json::parse(body());
      return cached_json_;
    }
    catch (const boost::system::system_error& e)
    {
      fmt::print(stderr, "Error parsing JSON body: {}\n", e.what());
      return std::nullopt;
    } catch (const std::exception& e)
    {
      fmt::print(stderr, "Unexpected error parsing JSON body: {}\n", e.what());
      return std::nullopt;
    }
  }

  void HttpContext::parse_form_params() const
  {
    if (form_params_parsed_)
    {
      return;
    }
    auto content_type = get_header(boost::beast::http::field::content_type);
    if (!content_type || content_type->find("application/x-www-form-urlencoded") == std::string::npos)
    {
      form_params_parsed_ = true;
      return;
    }
    auto query_string = "?" + body();
    boost::system::result<boost::urls::url_view> url_query_result = boost::urls::parse_relative_ref(query_string);

    if (url_query_result.has_value())
    {
      boost::urls::url_view url = url_query_result.value();
      for (const auto& param : url.params())
      {
        cached_form_params_[param.key] = param.value;
      }
    }
    else
    {
      fmt::print(stderr, "Error parsing x-www-form-urlencoded body: {}\n", url_query_result.error().message());
    }
    form_params_parsed_ = true;
  }

  std::optional<std::string> HttpContext::get_form_param(const std::string& key) const
  {
    parse_form_params();
    auto it = cached_form_params_.find(key);
    if (it != cached_form_params_.end())
    {
      return it->second;
    }
    return std::nullopt;
  }

  // Multipart/form-data 解析实现
  void HttpContext::parse_multipart_data() const
  {
    if (multipart_parsed_)
    {
      return;
    }

    auto content_type_header = get_header(boost::beast::http::field::content_type);
    if (!content_type_header || content_type_header->find("multipart/form-data") == std::string::npos)
    {
      multipart_parsed_ = true; // Not multipart, mark as parsed to avoid re-checking
      return;
    }

    // Extract boundary from Content-Type header
    size_t boundary_pos = content_type_header->find("boundary=");
    if (boundary_pos == std::string::npos)
    {
      fmt::print(stderr, "Multipart/form-data: No boundary found in Content-Type header.\n");
      multipart_parsed_ = true;
      return;
    }
    std::string boundary = content_type_header->substr(boundary_pos + std::string("boundary=").length());
    boundary = trim(boundary); // Trim potential quotes or whitespace

    std::string full_boundary = "--" + boundary;
    std::string final_boundary = full_boundary + "--";
    std::string body_str = body(); // Get the full request body as string

    size_t current_body_pos = 0;

    // Skip preamble before the first boundary
    current_body_pos = body_str.find(full_boundary);
    if (current_body_pos == std::string::npos)
    {
      fmt::print(stderr, "Multipart/form-data: First boundary not found.\n");
      multipart_parsed_ = true;
      return;
    }
    current_body_pos += full_boundary.length(); // Move past the first boundary

    // Loop through parts
    while (current_body_pos < body_str.length())
    {
      // Find end of headers (first \r\n\r\n after boundary)
      size_t header_end_pos = body_str.find("\r\n\r\n", current_body_pos);
      if (header_end_pos == std::string::npos)
      {
        fmt::print(stderr, "Multipart/form-data: Malformed part - no header end found.\n");
        break;
      }

      std::string part_headers = body_str.substr(current_body_pos, header_end_pos - current_body_pos);
      // Trim leading \r\n from headers if present (from previous boundary)
      if (part_headers.length() >= 2 && part_headers[0] == '\r' && part_headers[1] == '\n')
      {
        part_headers = part_headers.substr(2);
      }

      // Find start of next boundary (or final boundary)
      size_t next_boundary_pos = body_str.find(full_boundary, header_end_pos + 4); // +4 for \r\n\r\n
      if (next_boundary_pos == std::string::npos)
      {
        fmt::print(stderr, "Multipart/form-data: Next boundary not found. Malformed or premature end.\n");
        break; // Malformed or end of stream
      }

      std::string part_data = body_str.substr(header_end_pos + 4, next_boundary_pos - (header_end_pos + 4));
      // Trim trailing \r\n from data before boundary
      if (part_data.length() >= 2 && part_data[part_data.length() - 2] == '\r' && part_data[part_data.length() - 1] ==
        '\n')
      {
        part_data = part_data.substr(0, part_data.length() - 2);
      }

      // Parse Content-Disposition
      std::string content_disposition = extract_header_value(part_headers, "Content-Disposition");
      if (content_disposition.empty())
      {
        fmt::print(stderr, "Multipart/form-data: Part with no Content-Disposition header.\n");
        current_body_pos = next_boundary_pos + full_boundary.length();
        continue;
      }

      std::string name_str;
      std::string filename_str;

      std::regex disposition_regex("name=\"([^\"]+)\"(?:;\\s*filename=\"([^\"]+)\")?");
      std::smatch matches;
      if (std::regex_search(content_disposition, matches, disposition_regex))
      {
        if (matches.size() > 1)
        {
          name_str = matches[1].str();
        }
        if (matches.size() > 2 && matches[2].matched)
        {
          // Check if filename group was captured
          filename_str = matches[2].str();
        }
      }

      if (!name_str.empty())
      {
        if (!filename_str.empty())
        {
          // This is a file part
          MultipartFile file;
          file.filename = filename_str;
          file.content_type = extract_header_value(part_headers, "Content-Type");
          file.data = part_data;
          cached_multipart_files_[name_str].push_back(std::move(file));
        }
        else
        {
          // This is a regular form field
          cached_multipart_fields_[name_str] = part_data;
        }
      }

      current_body_pos = next_boundary_pos + full_boundary.length();
      if (body_str.substr(next_boundary_pos, final_boundary.length()) == final_boundary)
      {
        break; // Reached the final boundary
      }
    }

    multipart_parsed_ = true;
  }

  std::optional<std::string> HttpContext::get_multipart_field(const std::string& key) const
  {
    parse_multipart_data();
    auto it = cached_multipart_fields_.find(key);
    if (it != cached_multipart_fields_.end())
    {
      return it->second;
    }
    return std::nullopt;
  }

  const std::vector<MultipartFile>* HttpContext::get_uploaded_files(const std::string& field_name) const
  {
    parse_multipart_data();
    auto it = cached_multipart_files_.find(field_name);
    if (it != cached_multipart_files_.end())
    {
      return &it->second;
    }
    return nullptr; // or return std::optional<std::reference_wrapper<const std::vector<MultipartFile>>>
  }


  void HttpContext::set_status(boost::beast::http::status status)
  {
    res_.result(status);
  }

  void HttpContext::set_body(std::string body)
  {
    res_.body() = std::move(body);
    res_.prepare_payload();
  }

  void HttpContext::set_header(boost::beast::string_view name, boost::beast::string_view value)
  {
    res_.set(name, value);
  }

  void HttpContext::set_header(boost::beast::http::field name, boost::beast::string_view value)
  {
    set_header(boost::beast::http::to_string(name), value);
  }

  std::optional<std::string> HttpContext::get_header(boost::beast::http::field name) const
  {
    return get_header(boost::beast::http::to_string(name));
  }

  void HttpContext::set_content_type(boost::beast::string_view type)
  {
    res_.set(boost::beast::http::field::content_type, type);
  }

  void HttpContext::set_path_params(std::map<std::string, std::string> params) const
  {
    path_params_ = std::move(params);
  }
}
