#include "framework/client/http_client_stream.hpp"

#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <array>

namespace client = khttpd::framework::client;
namespace http = boost::beast::http;
namespace net = boost::asio;

TEST(HttpClientStreamEdgeTest, RejectsHttpsWithoutBufferedFallback)
{
  net::io_context ioc;
  auto stream = std::make_shared<client::HttpClientStream>(ioc);
  client::HttpClientStream::RequestHead head{http::verb::get, "/", 11};
  boost::system::error_code result;
  bool called = false;
  stream->async_start("https://example.test/path", std::move(head),
                      [&](boost::system::error_code ec) { result = ec; called = true; });
  ioc.run();
  EXPECT_TRUE(called);
  EXPECT_EQ(result, make_error_code(boost::system::errc::operation_not_supported));
}

TEST(HttpClientStreamEdgeTest, RejectsMalformedUrl)
{
  net::io_context ioc;
  auto stream = std::make_shared<client::HttpClientStream>(ioc);
  client::HttpClientStream::RequestHead head{http::verb::get, "/", 11};
  boost::system::error_code result;
  stream->async_start("not a url", std::move(head),
                      [&](boost::system::error_code ec) { result = ec; });
  ioc.run();
  EXPECT_EQ(result, make_error_code(boost::system::errc::operation_not_supported));
}

TEST(HttpClientStreamEdgeTest, WriteBeforeStartIsAborted)
{
  net::io_context ioc;
  auto stream = std::make_shared<client::HttpClientStream>(ioc);
  const std::array<char, 1> data{'x'};
  boost::system::error_code result;
  stream->async_write_some(net::buffer(data), [&](boost::system::error_code ec) { result = ec; });
  ioc.run();
  EXPECT_EQ(result, net::error::operation_aborted);
}

TEST(HttpClientStreamEdgeTest, FinishBeforeStartIsAborted)
{
  net::io_context ioc;
  auto stream = std::make_shared<client::HttpClientStream>(ioc);
  boost::system::error_code result;
  stream->async_finish_request([&](boost::system::error_code ec) { result = ec; });
  ioc.run();
  EXPECT_EQ(result, net::error::operation_aborted);
}
