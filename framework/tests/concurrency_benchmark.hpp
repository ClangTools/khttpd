#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace khttpd::framework::benchmark
{
  class Stats
  {
  public:
    Stats(std::vector<std::uint64_t> latencies_us,
          std::uint64_t elapsed_us,
          std::size_t successes,
          std::size_t failures)
      : latencies_us_(std::move(latencies_us)), elapsed_us_(elapsed_us), successes_(successes), failures_(failures)
    {
      std::sort(latencies_us_.begin(), latencies_us_.end());
    }

    double percentile(double fraction) const
    {
      if (latencies_us_.empty()) return 0.0;
      const auto bounded_fraction = std::max(0.0, std::min(1.0, fraction));
      const auto index = static_cast<std::size_t>(bounded_fraction * (latencies_us_.size() - 1) + 0.5);
      return static_cast<double>(latencies_us_[index]);
    }

    double requests_per_second() const
    {
      if (elapsed_us_ == 0) return 0.0;
      return static_cast<double>(successes_) * 1000000.0 / static_cast<double>(elapsed_us_);
    }

    double average_latency_us() const
    {
      if (latencies_us_.empty()) return 0.0;
      const auto total = std::accumulate(latencies_us_.begin(), latencies_us_.end(), std::uint64_t{0});
      return static_cast<double>(total) / static_cast<double>(latencies_us_.size());
    }

    std::size_t successes() const { return successes_; }
    std::size_t failures() const { return failures_; }

  private:
    std::vector<std::uint64_t> latencies_us_;
    std::uint64_t elapsed_us_;
    std::size_t successes_;
    std::size_t failures_;
  };

  inline std::string format_report(const Stats& stats, int server_threads, int concurrency, int requests)
  {
    const auto total = stats.successes() + stats.failures();
    const auto success_rate = total == 0 ? 0.0 : static_cast<double>(stats.successes()) * 100.0 / static_cast<double>(total);
    std::ostringstream report;
    report << "========================================\n"
           << "khttpd HTTP 并发基准测试\n"
           << "========================================\n"
           << "测试配置\n"
           << "  服务端线程数   : " << server_threads << "\n"
           << "  并发客户端数   : " << concurrency << "\n"
           << "  请求总数       : " << requests << "\n"
           << "----------------------------------------\n"
           << "测试结果\n"
           << "  成功请求数     : " << stats.successes() << "\n"
           << "  失败请求数     : " << stats.failures() << "\n"
           << "  成功率         : " << std::fixed << std::setprecision(2) << success_rate << "%\n"
           << "  吞吐量         : " << std::setprecision(2) << stats.requests_per_second() << " req/s\n"
           << "  平均延迟       : " << std::setprecision(2) << stats.average_latency_us() << " us\n"
           << "  p50 延迟       : " << stats.percentile(0.50) << " us\n"
           << "  p95 延迟       : " << stats.percentile(0.95) << " us\n"
           << "  p99 延迟       : " << stats.percentile(0.99) << " us\n"
           << "========================================\n";
    return report.str();
  }
}
