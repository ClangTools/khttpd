#ifndef KHTTPD_FRAMEWORK_CLIENT_IO_CONTEXT_POOL_HPP
#define KHTTPD_FRAMEWORK_CLIENT_IO_CONTEXT_POOL_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

namespace khttpd::framework
{
  class IoContextPool
  {
  public:
    // 获取单例实例
    static IoContextPool& instance(unsigned int num_threads = 0)
    {
      static IoContextPool instance{num_threads};
      return instance;
    }

    // 获取共享的 io_context
    boost::asio::io_context& get_io_context()
    {
      return ioc_;
    }

    // 获取当前运行的线程数量
    size_t get_thread_count() const
    {
      return threads_.size();
    }

    ~IoContextPool()
    {
      stop();
    }

    void stop()
    {
      // 确保只停止一次，防止析构和显式调用 stop 冲突
      std::call_once(stop_flag_, [this]()
      {
        work_guard_.reset(); // 允许 run() 退出
        ioc_.stop(); // 显式发出停止信号

        // 等待所有线程结束
        for (auto& t : threads_)
        {
          if (t.joinable())
          {
            t.join();
          }
        }
        threads_.clear();
      });
    }

  private:
    explicit IoContextPool(unsigned int count = std::thread::hardware_concurrency())
      : work_guard_(boost::asio::make_work_guard(ioc_))
    {
      // 如果检测失败（返回0）或者核心数少于1，保底使用 1 个线程
      // 如果为了提高并发吞吐量，也可以设为 count * 2
      if (count <= 0) count = 1;

      threads_.reserve(count * 2);

      // 2. 启动线程池
      for (unsigned int i = 0; i < count; ++i)
      {
        threads_.emplace_back([this]()
        {
          // 每个线程都运行同一个 io_context
          // ASIO 会自动调度 handler 到空闲线程
          ioc_.run();
        });
      }
    }


    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::vector<std::thread> threads_;
    std::once_flag stop_flag_;
  };
}

#endif // KHTTPD_FRAMEWORK_CLIENT_IO_CONTEXT_POOL_HPP
