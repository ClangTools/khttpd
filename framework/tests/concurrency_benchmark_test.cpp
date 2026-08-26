#include "framework/tests/concurrency_benchmark.hpp"

#include <gtest/gtest.h>

TEST(ConcurrencyBenchmarkStatsTest, CalculatesPercentilesAndThroughput)
{
  const khttpd::framework::benchmark::Stats stats({1, 2, 3, 4, 5}, 1000000, 4, 1);

  EXPECT_DOUBLE_EQ(stats.percentile(0.50), 3.0);
  EXPECT_DOUBLE_EQ(stats.percentile(0.95), 5.0);
  EXPECT_DOUBLE_EQ(stats.percentile(0.99), 5.0);
  EXPECT_DOUBLE_EQ(stats.requests_per_second(), 4.0);
}

TEST(ConcurrencyBenchmarkStatsTest, FormatsReadableChineseReport)
{
  const khttpd::framework::benchmark::Stats stats({100, 200, 300}, 1000000, 3, 0);
  const auto report = khttpd::framework::benchmark::format_report(stats, 4, 8, 80);

  EXPECT_NE(report.find("khttpd HTTP 并发基准测试"), std::string::npos);
  EXPECT_NE(report.find("并发客户端数"), std::string::npos);
  EXPECT_NE(report.find("吞吐量"), std::string::npos);
  EXPECT_NE(report.find("p95 延迟"), std::string::npos);
}
