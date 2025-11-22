#include <gtest/gtest.h>
#include "framework/router/http_router.hpp"
#include "framework/context/http_context.hpp"
#include "framework/interceptor/interceptor.hpp"
#include <memory>
#include <string>

using namespace khttpd::framework;

// Mock objects for HttpContext
// Since HttpContext constructor requires Request and Response objects
// We need to construct them.
class InterceptorTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    req.version(11);
    req.method(boost::beast::http::verb::get);
    req.target("/test");
    req.set(boost::beast::http::field::host, "localhost");
    req.prepare_payload();

    ctx = std::make_shared<HttpContext>(req, res);
    router = std::make_unique<HttpRouter>();
  }

  HttpContext::Request req;
  HttpContext::Response res;
  std::shared_ptr<HttpContext> ctx;
  std::unique_ptr<HttpRouter> router;
};

// Test implementation of Interceptor
class TestInterceptor : public Interceptor
{
public:
  InterceptorResult handle_request_result = InterceptorResult::Continue;
  bool handle_request_called = false;
  bool handle_response_called = false;
  std::string request_attribute_key;
  std::string request_attribute_value;

  InterceptorResult handle_request(HttpContext& ctx) override
  {
    handle_request_called = true;
    if (!request_attribute_key.empty())
    {
      ctx.set_attribute(request_attribute_key, request_attribute_value);
    }
    return handle_request_result;
  }

  void handle_response(HttpContext& ctx) override
  {
    handle_response_called = true;
    // Modify response header to prove it ran
    ctx.set_header("X-Test-Interceptor", "Executed");
  }
};

TEST_F(InterceptorTest, DefaultMethodsDoNothing)
{
  class DefaultInterceptor : public Interceptor
  {
  };
  auto interceptor = std::make_shared<DefaultInterceptor>();

  // Should return Continue by default
  EXPECT_EQ(interceptor->handle_request(*ctx), InterceptorResult::Continue);

  // Should do nothing (and compile/run)
  interceptor->handle_response(*ctx);
}

TEST_F(InterceptorTest, PreInterceptorPassThrough)
{
  auto interceptor = std::make_shared<TestInterceptor>();
  router->add_interceptor(interceptor);

  // Run pre-interceptors
  auto result = router->run_pre_interceptors(*ctx);

  EXPECT_EQ(result, InterceptorResult::Continue);
  EXPECT_TRUE(interceptor->handle_request_called);
}

TEST_F(InterceptorTest, PreInterceptorStop)
{
  auto interceptor = std::make_shared<TestInterceptor>();
  interceptor->handle_request_result = InterceptorResult::Stop;
  router->add_interceptor(interceptor);

  // Run pre-interceptors
  auto result = router->run_pre_interceptors(*ctx);

  EXPECT_EQ(result, InterceptorResult::Stop);
  EXPECT_TRUE(interceptor->handle_request_called);
}

TEST_F(InterceptorTest, MultipleInterceptorsSequence)
{
  auto i1 = std::make_shared<TestInterceptor>();
  auto i2 = std::make_shared<TestInterceptor>();

  router->add_interceptor(i1);
  router->add_interceptor(i2);

  auto result = router->run_pre_interceptors(*ctx);

  EXPECT_EQ(result, InterceptorResult::Continue);
  EXPECT_TRUE(i1->handle_request_called);
  EXPECT_TRUE(i2->handle_request_called);
}

TEST_F(InterceptorTest, MultipleInterceptorsStopMiddle)
{
  auto i1 = std::make_shared<TestInterceptor>();
  auto i2 = std::make_shared<TestInterceptor>();
  auto i3 = std::make_shared<TestInterceptor>();

  i2->handle_request_result = InterceptorResult::Stop;

  router->add_interceptor(i1);
  router->add_interceptor(i2);
  router->add_interceptor(i3);

  auto result = router->run_pre_interceptors(*ctx);

  EXPECT_EQ(result, InterceptorResult::Stop);
  EXPECT_TRUE(i1->handle_request_called);
  EXPECT_TRUE(i2->handle_request_called);
  EXPECT_FALSE(i3->handle_request_called); // Should not be called
}

TEST_F(InterceptorTest, PostInterceptorsRun)
{
  auto i1 = std::make_shared<TestInterceptor>();
  router->add_interceptor(i1);

  router->run_post_interceptors(*ctx);

  EXPECT_TRUE(i1->handle_response_called);
  auto header = ctx->get_response().find("X-Test-Interceptor");
  EXPECT_NE(header, ctx->get_response().end());
  EXPECT_EQ(header->value(), "Executed");
}

TEST_F(InterceptorTest, PostInterceptorsOrder)
{
  // Post interceptors typically run in reverse order or same order depending on implementation.
  // In HttpRouter::run_post_interceptors implementation:
  // for (auto it = interceptors_.rbegin(); it != interceptors_.rend(); ++it)
  // It runs in reverse order of addition.

  std::vector<int> order;

  class OrderingInterceptor : public Interceptor
  {
  public:
    int id;
    std::vector<int>& order_ref;

    OrderingInterceptor(int i, std::vector<int>& o) : id(i), order_ref(o)
    {
    }

    void handle_response(HttpContext& ctx) override
    {
      order_ref.push_back(id);
    }
  };

  auto i1 = std::make_shared<OrderingInterceptor>(1, order);
  auto i2 = std::make_shared<OrderingInterceptor>(2, order);

  router->add_interceptor(i1);
  router->add_interceptor(i2);

  router->run_post_interceptors(*ctx);

  // Expect 2 then 1 (Stack behavior / Onion model)
  ASSERT_EQ(order.size(), 2);
  EXPECT_EQ(order[0], 2);
  EXPECT_EQ(order[1], 1);
}

TEST_F(InterceptorTest, ContextDataPassing)
{
  auto i1 = std::make_shared<TestInterceptor>();
  i1->request_attribute_key = "user_id";
  i1->request_attribute_value = "12345";

  router->add_interceptor(i1);

  router->run_pre_interceptors(*ctx);

  auto val = ctx->get_attribute_as<std::string>("user_id");
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), "12345");
}
