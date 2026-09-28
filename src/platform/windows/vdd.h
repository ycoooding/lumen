/**
 * @file src/platform/windows/vdd.h
 * @brief Minimal control channel for the bundled ZakoVDD driver.
 */
#pragma once

#include <string>

namespace platf::vdd {
  bool ready();
  bool mode_available(const std::string &display_name, int width, int height, int fps);
  bool set_mode(int width, int height, int fps);
  bool create_monitor();
  bool destroy_monitor();
  bool owns_monitor();
}  // namespace platf::vdd
