/**
 * @file tests/unit/test_logging.cpp
 * @brief Test src/logging.*.
 */
#include "../tests_common.h"
#include "../tests_log_checker.h"

#include <format>
#include <random>
#include <src/logging.h>

extern boost::shared_ptr<text_sink> sink;

namespace {
  std::array log_levels = {
    std::tuple("verbose", &verbose),
    std::tuple("debug", &debug),
    std::tuple("info", &info),
    std::tuple("warning", &warning),
    std::tuple("error", &error),
    std::tuple("fatal", &fatal),
  };

  constexpr auto log_file = "test_sunshine.log";
}  // namespace

struct LogLevelsTest: testing::TestWithParam<decltype(log_levels)::value_type> {};

INSTANTIATE_TEST_SUITE_P(
  Logging,
  LogLevelsTest,
  testing::ValuesIn(log_levels),
  [](const auto &info) {
    return std::string(std::get<0>(info.param));
  }
);

TEST_P(LogLevelsTest, PutMessage) {
  auto [label, plogger] = GetParam();
  ASSERT_TRUE(plogger);
  auto &logger = *plogger;

  std::random_device rand_dev;
  std::mt19937_64 rand_gen(rand_dev());
  auto test_message = std::format("{}{}", rand_gen(), rand_gen());
  BOOST_LOG(logger) << test_message;

  ASSERT_TRUE(log_checker::line_contains(log_file, test_message));
}

TEST(Logging, NoneFiltersProductionLogLevels) {
  constexpr auto none_log_file = "test_sunshine_none.log";
  constexpr auto test_message = "none-filter-test";
  auto original_sink = sink;
  auto none_log_guard = logging::init(6, none_log_file);

  for (const auto &[_, plogger] : log_levels) {
    BOOST_LOG(*plogger) << test_message;
  }

  EXPECT_FALSE(log_checker::line_contains(none_log_file, test_message));

  none_log_guard.reset();
  sink = original_sink;
  boost::log::core::get()->add_sink(sink);
#ifndef __ANDROID__
  logging::setup_av_logging(0);
  logging::setup_libdisplaydevice_logging(0);
#endif
}
