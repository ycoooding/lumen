/**
 * @file src/platform/windows/vdd.h
 * @brief Minimal control channel for the bundled ZakoVDD driver.
 */
#pragma once

namespace platf::vdd {
  bool set_mode(int width, int height, int fps);
  bool create_monitor();
  bool destroy_monitor();
  bool owns_monitor();
}  // namespace platf::vdd
