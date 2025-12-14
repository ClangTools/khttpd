#ifndef KHTTPD_FRAMEWORK_CRON_SCHEDULER_HPP
#define KHTTPD_FRAMEWORK_CRON_SCHEDULER_HPP

#include "CronJob.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

namespace khttpd::framework
{
  class CronScheduler
  {
  private:
    // 内部通用实现类：LambdaCronJob
    // 专门用于执行 std::function 的任务
    class LambdaCronJob : public CronJob
    {
    public:
      LambdaCronJob(const std::string& expr, std::function<void()> func)
        : CronJob(expr), func_(std::move(func))
      {
      }

    protected:
      void run() override
      {
        if (func_) func_();
      }

    private:
      std::function<void()> func_;
    };

  public:
    // 单例获取
    static CronScheduler& instance()
    {
      static CronScheduler instance;
      return instance;
    }

    // 禁止拷贝
    CronScheduler(const CronScheduler&) = delete;
    CronScheduler& operator=(const CronScheduler&) = delete;

    /**
     * @brief 调度一个 Cron 任务
     *
     * @param expression Cron 表达式 (如 "* * * * * *")
     * @param task 回调函数
     * @param delay_ms 首次启动延迟 (默认 0)
     * @return std::shared_ptr<CronJob> 返回任务句柄。
     *         注意：即使忽略返回值，任务也会自动运行（因为 ASIO 内部持有了 shared_ptr）。
     *         保留返回值是为了让你有机会调用 .stop()。
     */
    std::shared_ptr<CronJob> schedule(
      const std::string& expression,
      std::function<void()> task,
      std::chrono::milliseconds delay_ms = std::chrono::milliseconds(0))
    {
      auto job = std::make_shared<LambdaCronJob>(expression, std::move(task));
      job->start(delay_ms);

      // 这里的 job 即使出了作用域，也会因为 CronJob 内部 async_wait 捕获了 shared_from_this 而存活。
      // 返回它只是为了让调用者有控制权。
      return job;
    }

  private:
    CronScheduler() = default;
  };
}

#endif // KHTTPD_FRAMEWORK_CRON_SCHEDULER_HPP
