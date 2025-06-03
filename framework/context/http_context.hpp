#ifndef KHTTPD_FRAMEWORK_CONTEXT_HTTP_CONTEXT_HPP_
#define KHTTPD_FRAMEWORK_CONTEXT_HTTP_CONTEXT_HPP_

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/parse.hpp>
#include <boost/json.hpp>
#include <string>
#include <map>
#include <optional>
#include <vector>
#include <regex> // For Content-Disposition parsing

namespace khttpd::framework
{
  struct MultipartFile
  {
    std::string filename;
    std::string content_type;
    std::string data;
  };


  class HttpContext
  {
  public:
    using Request = boost::beast::http::request<boost::beast::http::string_body>;
    using Response = boost::beast::http::response<boost::beast::http::string_body>;

    HttpContext(Request& req, Response& res);

    const std::string& path() const;
    boost::beast::http::verb method() const;
    std::string body() const;
    std::optional<std::string> get_query_param(const std::string& key) const;
    std::optional<std::string> get_path_param(const std::string& key) const;
    std::optional<std::string> get_header(boost::beast::string_view name) const;
    std::optional<std::string> get_header(boost::beast::http::field name) const;

    std::optional<boost::json::value> get_json() const;
    std::optional<std::string> get_form_param(const std::string& key) const;

    std::optional<std::string> get_multipart_field(const std::string& key) const;
    const std::vector<MultipartFile>* get_uploaded_files(const std::string& field_name) const;


    void set_status(boost::beast::http::status status);
    void set_body(std::string body);
    void set_header(boost::beast::string_view name, boost::beast::string_view value);
    void set_header(boost::beast::http::field name, boost::beast::string_view value);
    void set_content_type(boost::beast::string_view type);

    Request& get_request() { return req_; }
    Response& get_response() { return res_; }

    void set_path_params(std::map<std::string, std::string> params) const;

  private:
    Request& req_;
    Response& res_;
    mutable std::map<std::string, std::string> query_params_;
    mutable std::string cached_path_;
    mutable boost::urls::url_view parsed_url_;
    mutable bool url_parsed_ = false;

    mutable std::map<std::string, std::string> path_params_;

    mutable std::optional<boost::json::value> cached_json_;
    mutable std::map<std::string, std::string> cached_form_params_;
    mutable bool form_params_parsed_ = false;

    mutable std::map<std::string, std::string> cached_multipart_fields_;
    mutable std::map<std::string, std::vector<MultipartFile>> cached_multipart_files_;
    mutable bool multipart_parsed_ = false;

    void parse_url_components() const;
    void parse_form_params() const;
    void parse_multipart_data() const;

    std::string trim(const std::string& str) const;
    std::string extract_header_value(const std::string& headers, const std::string& header_name) const;
  };
}
#endif // KHTTPD_FRAMEWORK_CONTEXT_HTTP_CONTEXT_HPP_