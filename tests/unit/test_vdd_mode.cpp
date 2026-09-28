#include "../tests_common.h"

#include <src/platform/windows/vdd_mode.h>

TEST(VddMode, PreservesConfiguredModesAndAddsClientMode) {
  EXPECT_EQ(platf::vdd::mode_command({{1920, 1080, 60}, {2560, 1440, 144}}, {2560, 1600, 120}), L"SETMODES 1920x1080x60,2560x1440x144,2560x1600x120");
}

TEST(VddMode, RepeatedRequestsDoNotDuplicateModes) {
  EXPECT_EQ(platf::vdd::mode_command({{1920, 1080, 60}, {1920, 1080, 60}}, {1920, 1080, 60}), L"SETMODES 1920x1080x60");
}

TEST(VddMode, RejectsInvalidClientModes) {
  for (const auto &mode : std::vector<platf::vdd::mode_t> {{0, 1080, 60}, {1920, -1, 60}, {1920, 1080, 0}, {16385, 1080, 60}, {1920, 1080, 1001}}) {
    EXPECT_EQ(platf::vdd::mode_command({}, mode), std::nullopt);
  }
}

TEST(VddMode, RejectsOverflowInsteadOfSendingTruncatedModeList) {
  std::vector<platf::vdd::mode_t> modes;
  for (int fps = 1; fps <= 300; ++fps) {
    modes.push_back({1920, 1080, fps});
  }
  EXPECT_EQ(platf::vdd::mode_command(modes, {2560, 1440, 144}), std::nullopt);
}
