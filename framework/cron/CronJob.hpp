#ifndef KHTTPD_FRAMEWORK_CRON_JOB_HPP
#define KHTTPD_FRAMEWORK_CRON_JOB_HPP

#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <ctime>
#include <atomic> // 引入 atomic
#include <boost/asio.hpp>
#include "croncpp.hpp"
#include "io_context_pool.hpp"

namespace khttpd::framework
{
  class CronJob : public std::enable_shared_from_this<CronJob>
  {
  public:
    explicit CronJob(const std::string& expression)
      : timer_(IoContextPool::instance().get_io_context())
        , expression_(expression)
        , is_running_(false) // 初始化为 false
    {
      try
      {
        cron_expr_ = cron::make_cron(expression);
      }
      catch (const std::exception& e)
      {
        std::cerr << "[CronJob] Invalid expression '" << expression << "': " << e.what() << std::endl;
        throw;
      }
    }

    virtual ~CronJob()
    {
    }

    void start()
    {
      // 防止重复启动
      bool expected = false;
      if (is_running_.compare_exchange_strong(expected, true))
      {
        schedule_next();
      }
    }

    void stop()
    {
      // 1. 先修改状态位，这是最重要的！
      // 即使后面的 cancel 没能阻止当前回调，回调里也会检查这个标志位
      is_running_ = false;

      // 2. 尝试取消当前的等待
      timer_.cancel();
    }

  protected:
    virtual void run() = 0;

  private:
    void schedule_next()
    {
      // 如果已经停止，就不再计算下一次了
      if (!is_running_) return;

      auto now_time_t = std::time(nullptr);
      std::time_t next_time_t = cron::cron_next(cron_expr_, now_time_t);
      auto next_time_point = std::chrono::system_clock::from_time_t(next_time_t);

      timer_.expires_at(next_time_point);

      auto self = shared_from_this();

      timer_.async_wait([this, self](const boost::system::error_code& ec)
      {
        // 检查 1: 如果被显式 Cancel (operation_aborted)，直接退出
        if (ec == boost::asio::error::operation_aborted) return;

        // 检查 2: 双重保险。
        // 如果 stop() 在回调入队后被调用，ec 可能是 success，但 is_running_ 已经是 false 了
        if (!is_running_) return;

        if (ec)
        {
          std::cerr << "[CronJob] Timer error: " << ec.message() << std::endl;
          return;
        }

        try
        {
          this->run();
        }
        catch (const std::exception& e)
        {
          std::cerr << "[CronJob] Task exception: " << e.what() << std::endl;
        }

        // 检查 3: 再次确认。
        // 有可能在 run() 执行期间，外部调用了 stop()。
        // 如果这里不检查，任务会再次复活。
        if (is_running_)
        {
          schedule_next();
        }
      });
    }

  private:
    boost::asio::system_timer timer_;
    std::string expression_;
    cron::cronexpr cron_expr_;
    std::atomic<bool> is_running_; // 关键修改
  };
}

#endif
