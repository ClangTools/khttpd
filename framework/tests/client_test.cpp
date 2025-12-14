#include "framework/client/http_client.hpp"
#include "framework/client/websocket_client.hpp"
#include <gtest/gtest.h>
#include <boost/json.hpp>
#include <map>
#include <thread>
#include <iostream>

using namespace khttpd::framework::client;
namespace http = boost::beast::http;

// ==========================================
// 1. 定义 PostmanEchoClient 类
// ==========================================
class PostmanEchoClient : public HttpClient
{
public:
  // 构造函数：注入 ioc，并设置默认 Base URL
  PostmanEchoClient(boost::asio::io_context& ioc)
    : HttpClient(ioc)
  {
    set_base_url("https://postman-echo.com");
    // 设置一个较长的超时时间，防止 CI 环境网络慢
    set_timeout(std::chrono::seconds(10));
  }

  // ------------------------------------------------------------------
  // API 定义
  // ------------------------------------------------------------------

  // 1. GET 请求，带查询参数
  // Endpoint: /get?foo=bar
  API_CALL(http::verb::get, "/get", echo_get,
           QUERY(std::string, foo_val, "foo"),
           QUERY(int, id_val, "id"))

  // 2. POST 请求，带 JSON Body
  // Endpoint: /post
  API_CALL(http::verb::post, "/post", echo_post,
           BODY(boost::json::object, json_body))

  // 3. GET 请求，测试 Header 传递
  // Endpoint: /headers
  // 我们定义一个名为 request_id 的参数，它会被映射为 HTTP Header "X-Request-Id"
  API_CALL(http::verb::get, "/headers", echo_headers,
           HEADER(std::string, request_id, "X-My-Request-Id"),
           HEADER(std::string, user_token, "X-User-Token"))

  // 4. PUT 请求，带路径参数
  // Endpoint: /put (Postman echo 实际上忽略路径后的东西，但我们可以测试 URL 拼接)
  API_CALL(http::verb::put, "/put", echo_put_dummy)
};

// ==========================================
// 2. 测试用例
// ==========================================

class ClientTest : public ::testing::Test
{
protected:
  boost::asio::io_context ioc;
  std::shared_ptr<PostmanEchoClient> client;

  // 辅助：用于在主线程等待异步结果
  void run_until_complete()
  {
    ioc.run();
    ioc.restart(); // 重置以便下次使用
  }

  void SetUp() override
  {
    client = std::make_shared<PostmanEchoClient>(ioc);
  }
};

// 测试 1: GET Query 参数
TEST_F(ClientTest, GetWithQueryParams)
{
  bool done = false;

  // 调用: /get?foo=hello&id=123
  client->echo_get("hello", 123, [&](auto ec, auto res)
  {
    if (!ec)
    {
      EXPECT_EQ(res.result(), http::status::ok);
      std::string body = res.body();
      // 验证 Postman Echo 返回的 args json
      EXPECT_TRUE(body.find("\"foo\":\"hello\"") != std::string::npos) << "Body: " << body;
      EXPECT_TRUE(body.find("\"id\":\"123\"") != std::string::npos) << "Body: " << body;
    }
    else
    {
      ADD_FAILURE() << "Network error: " << ec.message();
    }
    done = true;
  });

  run_until_complete();
  EXPECT_TRUE(done);
}

// 测试 2: POST JSON Body
TEST_F(ClientTest, PostJsonBody)
{
  bool done = false;
  boost::json::object jv;
  jv["message"] = "test_payload";
  jv["count"] = 99;

  client->echo_post(jv, [&](auto ec, auto res)
  {
    if (!ec)
    {
      EXPECT_EQ(res.result(), http::status::ok);
      std::string body = res.body();
      // 验证 data 字段
      EXPECT_TRUE(body.find("test_payload") != std::string::npos) << "Body: " << body;
      EXPECT_TRUE(body.find("99") != std::string::npos) << "Body: " << body;
    }
    else
    {
      ADD_FAILURE() << "Network error: " << ec.message();
    }
    done = true;
  });

  run_until_complete();
  EXPECT_TRUE(done);
}

// 测试 3: Headers 传递
TEST_F(ClientTest, CustomHeaders)
{
  bool done = false;
  std::string rid = "req-unique-id-001";
  std::string token = "secret-token-abc";

  // 传递 Header 参数
  client->echo_headers(rid, token, [&](auto ec, auto res)
  {
    if (!ec)
    {
      EXPECT_EQ(res.result(), http::status::ok);
      std::string body = res.body();

      // Postman Echo 返回的 headers key 都是小写的
      // 注意：这里要匹配小写，因为 HTTP/2 或部分 HTTP/1.x 实现会将 header key 规范化为小写
      bool has_rid = body.find("x-my-request-id") != std::string::npos ||
        body.find("X-My-Request-Id") != std::string::npos;

      bool has_val = body.find(rid) != std::string::npos;

      bool has_token = body.find("secret-token-abc") != std::string::npos;

      if (!has_rid || !has_val || !has_token)
      {
        std::cerr << ">>> TEST FAILURE DEBUG INFO <<<" << std::endl;
        std::cerr << "Expected Value: " << rid << std::endl;
        std::cerr << "Actual Response Body: \n" << body << std::endl;
      }

      EXPECT_TRUE(has_rid) << "Missing Header Key: X-My-Request-Id";
      EXPECT_TRUE(has_val) << "Missing Header Value: " << rid;
      EXPECT_TRUE(has_token) << "Missing X-User-Token value";
    }
    else
    {
      ADD_FAILURE() << "Network error: " << ec.message();
    }
    done = true;
  });

  run_until_complete();
  EXPECT_TRUE(done);
}

// 测试 4: Sync 同步调用 (带 Base URL)
TEST_F(ClientTest, SyncCall)
{
  // 重要：同步调用会阻塞当前线程等待 future，
  // 所以 io_context 必须在另一个线程跑，否则死锁。
  auto work = boost::asio::make_work_guard(ioc);
  std::thread ioc_thread([&]
  {
    ioc.run();
  });

  try
  {
    // 使用同步生成的 API
    auto res = client->echo_get_sync("sync_world", 999);

    EXPECT_EQ(res.result(), http::status::ok);
    std::string body = res.body();
    EXPECT_TRUE(body.find("sync_world") != std::string::npos);
  }
  catch (const std::exception& e)
  {
    ADD_FAILURE() << "Sync request exception: " << e.what();
  }

  // 清理
  work.reset();
  ioc.stop();
  if (ioc_thread.joinable()) ioc_thread.join();
}

// 测试 5: 全局默认 Header
TEST_F(ClientTest, GlobalDefaultHeader)
{
  bool done = false;
  // 设置一个全局 Header，所有请求都应该带上
  client->set_default_header("X-App-Version", "v1.0.0-beta");

  // 复用 echo_headers 接口，参数传空字符串看看默认 header 是否还在
  client->echo_headers("id-1", "token-1", [&](auto ec, auto res)
  {
    if (!ec)
    {
      std::string body = res.body();
      // 检查全局 Header 是否被服务器收到
      EXPECT_TRUE(body.find("v1.0.0-beta") != std::string::npos)
                << "Global default header missing in: " << body;
    }
    done = true;
  });

  run_until_complete();
  EXPECT_TRUE(done);
}

// ==========================================
// WebSocket 测试
// ==========================================

// ==========================================
// WebSocket 测试
// ==========================================

class WebsocketTest : public ::testing::Test
{
protected:
  boost::asio::io_context ioc;
  std::shared_ptr<WebsocketClient> ws_client;

  void SetUp() override
  {
    ws_client = std::make_shared<WebsocketClient>(ioc);
  }

  void TearDown() override
  {
    if (ws_client) ws_client->close();
  }
};

TEST_F(WebsocketTest, WssEchoAndWriteQueue)
{
  std::string url = "wss://echo.websocket.org";

  const int message_count = 5;
  int received_count = 0;
  bool closed_gracefully = false;

  // 增加一个 flag 标记是否发生严重错误
  bool has_error = false;

  // 创建定时器，但先不 async_wait，后面逻辑控制
  boost::asio::steady_timer timer(ioc, std::chrono::seconds(15));

  ws_client->set_on_message([&](const std::string& msg)
  {
    // 过滤欢迎消息
    if (msg.find("Request served by") != std::string::npos) return;

    received_count++;
    // std::cout << "Msg: " << msg << std::endl;

    if (received_count >= message_count)
    {
      ws_client->close();
    }
  });

  ws_client->set_on_close([&]()
  {
    closed_gracefully = true;
    // 关键：连接关闭后，取消定时器，ioc.run() 就会立即返回
    timer.cancel();
  });

  ws_client->set_on_error([&](boost::beast::error_code ec)
  {
    // 忽略操作取消（通常是 close() 导致的 pending read 取消）
    if (ec == boost::asio::error::operation_aborted) return;

    std::cerr << "WS Error: " << ec.message() << std::endl;
    has_error = true;
    timer.cancel(); // 发生错误也停止测试
  });

  ws_client->connect(url, [&](boost::beast::error_code ec)
  {
    if (ec)
    {
      ADD_FAILURE() << "WS Connect Failed: " << ec.message();
      timer.cancel();
      return;
    }

    for (int i = 0; i < message_count; ++i)
    {
      ws_client->send("Msg-" + std::to_string(i));
    }
  });

  // 启动超时计时
  timer.async_wait([&](boost::system::error_code ec)
  {
    if (ec == boost::asio::error::operation_aborted)
    {
      // 定时器被取消，说明测试正常结束或提前出错
      return;
    }
    // 定时器真的触发了 -> 超时
    ws_client->close();
    ADD_FAILURE() << "Test Timed Out! Received: " << received_count << "/" << message_count;
  });

  ioc.run();

  EXPECT_FALSE(has_error) << "Should not encounter network errors";
  EXPECT_EQ(received_count, message_count);
  EXPECT_TRUE(closed_gracefully) << "on_close should be triggered";
}

TEST_F(WebsocketTest, ConnectFailure)
{
  // 测试连接不可达端口
  bool failed = false;
  ws_client->connect("ws://localhost:59999", [&](boost::beast::error_code ec)
  {
    if (ec)
    {
      failed = true;
    }
  });

  ioc.run();
  EXPECT_TRUE(failed);
}
