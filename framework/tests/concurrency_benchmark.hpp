#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
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
}
