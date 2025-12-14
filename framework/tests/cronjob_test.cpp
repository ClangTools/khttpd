#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

// 包含你之前的头文件
#include "cron/CronJob.hpp"
#include "io_context_pool.hpp"

using namespace khttpd::framework;

// --- 测试用的辅助类 ---
class TestableCronJob : public CronJob
{
public:
  TestableCronJob(const std::string& expr)
    : CronJob(expr), run_count_(0)
  {
  }

  // 实现 run 方法
  void run() override
  {
    // 1. 增加计数
    run_count_++;

    // 2. 通知测试线程
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // 只需要通知，具体逻辑由测试线程判断
    }
    cv_.notify_one();
  }

  // 辅助方法：等待任务执行 n 次
  // 返回 true 表示在超时前完成了任务，false 表示超时
  bool wait_for_runs(int expected_count, std::chrono::milliseconds timeout)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [this, expected_count]()
    {
      return run_count_ >= expected_count;
    });
  }

  int get_run_count() const
  {
    return run_count_;
  }

private:
  std::atomic<int> run_count_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

// --- 测试套件 ---

class CronJobTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    // 确保 IoContextPool 至少有一个线程在运行
    // 注意：单例模式下，这个池会在所有测试间共享
    IoContextPool::instance(1);
  }

  static void TearDownTestSuite()
  {
    // 测试结束后停止池（可选，视具体需求而定）
    // IoContextPool::instance().stop();
  }
};

// 测试 1: 验证无效的 Cron 表达式会抛出异常
TEST_F(CronJobTest, ThrowsOnInvalidExpression)
{
  // 这是一个错误的表达式 (只有 5 个字段，或者是乱码)
  std::string invalid_expr = "invalid cron string";

  EXPECT_THROW({
               auto job = std::make_shared<TestableCronJob>(invalid_expr);
               }, std::runtime_error); // 这里的异常类型取决于 croncpp 具体抛出什么，通常是 std::runtime_error 或 croncpp::cron_exception
}

// 测试 2: 验证任务是否能被调度和执行
TEST_F(CronJobTest, RunsScheduleCorrectly)
{
  // 设置为每秒执行一次 ("* * * * * *")
  // 注意：croncpp 能够处理秒级
  auto job = std::make_shared<TestableCronJob>("* * * * * *");

  job->start();

  // 等待任务至少执行 1 次
  // 给它 2.5 秒的时间（理论上应该在第 1 秒或第 2 秒触发）
  bool executed = job->wait_for_runs(1, std::chrono::milliseconds(2500));

  EXPECT_TRUE(executed) << "Job did not run within timeout";
  EXPECT_GE(job->get_run_count(), 1);

  job->stop();
}

// 测试 3: 验证 Stop 后不再执行
TEST_F(CronJobTest, StopPreventsFurtherExecution)
{
  auto job = std::make_shared<TestableCronJob>("* * * * * *");
  job->start();

  // 等待第 1 次
  ASSERT_TRUE(job->wait_for_runs(1, std::chrono::seconds(2)));

  // 停止
  job->stop();

  // 获取当前快照
  int count_after_stop = job->get_run_count();

  // 再等一会儿，看会不会偷偷跑
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // 现在这里应该能通过了
  // 即使在 stop 瞬间正好有一次执行完成，检查3也会阻止下一次调度
  EXPECT_EQ(job->get_run_count(), count_after_stop);
}

// 测试 4: 多个任务并发
TEST_F(CronJobTest, MultipleJobs)
{
  auto job1 = std::make_shared<TestableCronJob>("* * * * * *");
  auto job2 = std::make_shared<TestableCronJob>("* * * * * *");

  job1->start();
  job2->start();

  // 等待两个任务都至少运行一次
  EXPECT_TRUE(job1->wait_for_runs(1, std::chrono::seconds(2)));
  EXPECT_TRUE(job2->wait_for_runs(1, std::chrono::seconds(2)));

  job1->stop();
  job2->stop();
}
