#include "http_session_test_harness.hpp"

#include <gtest/gtest.h>
#include <array>

namespace fw = khttpd::framework;
namespace test = khttpd::framework::tests;
namespace http = boost::beast::http;
namespace net = boost::asio;

namespace
{
  void install_counting_stream(fw::HttpRouter& router, std::size_t read_buffer_size = 5)
  {
    router.stream("/stream", http::verb::post,
      [read_buffer_size](fw::HttpContext& ctx, std::shared_ptr<fw::HttpRequestStream> request,
                         std::shared_ptr<fw::HttpResponseStream>, fw::HttpStreamComplete complete)
      {
        struct State
        {
          std::vector<char> buffer;
          std::size_t total = 0;
          fw::HttpContext* ctx;
          std::shared_ptr<fw::HttpRequestStream> request;
          fw::HttpStreamComplete complete;
          std::function<void()> read;
        };
        auto state = std::make_shared<State>();
        state->buffer.resize(read_buffer_size);
        state->ctx = &ctx;
        state->request = std::move(request);
        state->complete = std::move(complete);
        state->read = [state]
        {
          state->request->async_read_some(net::buffer(state->buffer),
            [state](boost::system::error_code ec, std::size_t size, bool done)
            {
              ASSERT_FALSE(ec) << ec.message();
              state->total += size;
              if (!done) return state->read();
              state->ctx->set_body(std::to_string(state->total));
              state->complete();
            });
        };
        state->read();
      });
  }

  http::response<http::string_body> send_stream(std::string body, bool chunked,
                                                 std::uint64_t buffered_limit = 0)
  {
    test::TempWebRoot web_root;
    fw::HttpRouter router;
    fw::WebsocketRouter websocket_router;
    install_counting_stream(router);
    http::request<http::string_body> request{http::verb::post, "/stream", 11};
    request.body() = std::move(body);
    if (chunked) request.chunked(true); else request.prepare_payload();
    request.keep_alive(false);
    return test::round_trip(router, websocket_router, web_root.path, std::move(request), buffered_limit);
  }
}

TEST(HttpStreamEdgeTest, EmptyContentLengthBodyCompletesWithoutReadData)
{
  const auto response = send_stream({}, false);
  EXPECT_EQ(response.result(), http::status::ok);
  EXPECT_EQ(response.body(), "0");
}

TEST(HttpStreamEdgeTest, StreamRouteBypassesZeroBufferedBodyLimit)
{
  const auto response = send_stream(std::string(4097, 's'), false, 0);
  EXPECT_EQ(response.result(), http::status::ok);
  EXPECT_EQ(response.body(), "4097");
}

TEST(HttpStreamEdgeTest, ChunkedBodyCrossingManySmallReadsPreservesByteCount)
{
  const auto response = send_stream(std::string(103, 'c'), true, 1);
  EXPECT_EQ(response.result(), http::status::ok);
  EXPECT_EQ(response.body(), "103");
}
