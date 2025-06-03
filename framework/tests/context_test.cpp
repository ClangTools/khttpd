#include "framework/context/http_context.hpp"
#include <gtest/gtest.h>
#include <boost/beast/http/empty_body.hpp> // For empty body requests

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace khttpd_fw = khttpd::framework;

// Helper function to create a request for testing
template <class Body = http::string_body>
http::request<Body> make_request(
  http::verb method,
  const std::string& target,
  int version = 11,
  const std::string& body_str = "")
{
  http::request<Body> req(method, target, version);
  if (!body_str.empty())
  {
    req.body() = body_str;
    req.prepare_payload();
  }
  return req;
}

// Helper to create HttpContext with dummy response
khttpd_fw::HttpContext create_context(
  http::request<http::string_body>& req,
  http::response<http::string_body>& res)
{
  return khttpd_fw::HttpContext(req, res);
}

TEST(HttpContextTest, PathAndMethod)
{
  http::request<http::string_body> req = make_request(http::verb::get, "/users/123?q=test");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_EQ(ctx.path(), "/users/123");
  ASSERT_EQ(ctx.method(), http::verb::get);
}

TEST(HttpContextTest, QueryParameters)
{
  http::request<http::string_body> req = make_request(http::verb::get, "/search?query=boost+beast&page=2");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_TRUE(ctx.get_query_param("query").has_value());
  ASSERT_EQ(ctx.get_query_param("query").value(), "boost beast");

  ASSERT_TRUE(ctx.get_query_param("page").has_value());
  ASSERT_EQ(ctx.get_query_param("page").value(), "2");

  ASSERT_FALSE(ctx.get_query_param("non_existent").has_value());
}

TEST(HttpContextTest, Headers)
{
  http::request<http::string_body> req = make_request(http::verb::get, "/");
  req.set(http::field::user_agent, "Test-Agent/1.0");
  req.set("X-Custom-Header", "CustomValue");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_TRUE(ctx.get_header(http::field::user_agent).has_value());
  ASSERT_EQ(ctx.get_header(http::field::user_agent).value(), "Test-Agent/1.0");

  ASSERT_TRUE(ctx.get_header("X-Custom-Header").has_value());
  ASSERT_EQ(ctx.get_header("X-Custom-Header").value(), "CustomValue");

  ASSERT_FALSE(ctx.get_header("Non-Existent-Header").has_value());
}

TEST(HttpContextTest, JsonBody)
{
  std::string json_str = R"({"name": "Alice", "age": 30})";
  http::request<http::string_body> req = make_request(http::verb::post, "/api/json", 11, json_str);
  req.set(http::field::content_type, "application/json");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_EQ(ctx.body(), json_str);
  ASSERT_TRUE(ctx.get_json().has_value());
  ASSERT_TRUE(ctx.get_json().value().is_object());
  ASSERT_EQ(ctx.get_json().value().at("name").as_string(), "Alice");
  ASSERT_EQ(ctx.get_json().value().at("age").as_int64(), 30);
}

TEST(HttpContextTest, InvalidJsonBody)
{
  std::string invalid_json_str = R"({"name": "Alice", "age": })"; // Malformed JSON
  http::request<http::string_body> req = make_request(http::verb::post, "/api/json", 11, invalid_json_str);
  req.set(http::field::content_type, "application/json");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_FALSE(ctx.get_json().has_value());
}

TEST(HttpContextTest, FormUrlEncodedBody)
{
  std::string form_str = "param1=value1&param2=value%202";
  http::request<http::string_body> req = make_request(http::verb::post, "/api/form", 11, form_str);
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_TRUE(ctx.get_form_param("param1").has_value());
  ASSERT_EQ(ctx.get_form_param("param1").value(), "value1");
  ASSERT_TRUE(ctx.get_form_param("param2").has_value());
  ASSERT_EQ(ctx.get_form_param("param2").value(), "value 2");
  ASSERT_FALSE(ctx.get_form_param("non_existent").has_value());
}

TEST(HttpContextTest, MultipartFormData)
{
  std::string boundary = "----------WebKitFormBoundary12345";
  std::string multipart_body =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"description\"\r\n\r\n"
    "This is a test description.\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"my_test.txt\"\r\n"
    "Content-Type: text/plain\r\n\r\n"
    "Hello, world!\r\n"
    "--" + boundary + "--\r\n";

  http::request<http::string_body> req = make_request(http::verb::post, "/api/upload", 11, multipart_body);
  req.set(http::field::content_type, "multipart/form-data; boundary=" + boundary);
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ASSERT_TRUE(ctx.get_multipart_field("description").has_value());
  ASSERT_EQ(ctx.get_multipart_field("description").value(), "This is a test description.");

  const auto* files = ctx.get_uploaded_files("file");
  ASSERT_NE(files, nullptr);
  ASSERT_EQ(files->size(), 1);
  ASSERT_EQ(files->at(0).filename, "my_test.txt");
  ASSERT_EQ(files->at(0).content_type, "text/plain");
  ASSERT_EQ(files->at(0).data, "Hello, world!");

  ASSERT_EQ(ctx.get_uploaded_files("non_existent_file_field"), nullptr);
  ASSERT_FALSE(ctx.get_multipart_field("non_existent_field").has_value());
}

// Test response setters
TEST(HttpContextTest, ResponseSetters)
{
  http::request<http::string_body> req = make_request(http::verb::get, "/");
  http::response<http::string_body> res;
  khttpd_fw::HttpContext ctx = create_context(req, res);

  ctx.set_status(http::status::not_found);
  ctx.set_body("Custom 404");
  ctx.set_content_type("text/html");
  ctx.set_header("X-Framework-Version", "1.0");

  // Access the raw response from context to verify
  auto& actual_res = ctx.get_response();

  ASSERT_EQ(actual_res.result(), http::status::not_found);
  ASSERT_EQ(actual_res.body(), "Custom 404");
  ASSERT_EQ(actual_res[http::field::content_type], "text/html");
  ASSERT_EQ(actual_res["X-Framework-Version"], "1.0");
}
