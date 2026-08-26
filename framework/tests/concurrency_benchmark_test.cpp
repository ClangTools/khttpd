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
