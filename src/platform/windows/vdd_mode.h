/**
 * @file src/platform/windows/vdd_mode.h
 * @brief Build the bounded ZakoVDD live mode-list command without dropping configured modes.
 */
#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace platf::vdd {
  struct mode_t {
    int width;
    int height;
    int fps;
    bool operator==(const mode_t &) const = default;
  };

  inline bool valid_mode(const mode_t &mode) {
    return mode.width > 0 && mode.height > 0 && mode.width <= 16384 &&
           mode.height <= 16384 && mode.fps > 0 && mode.fps <= 1000;
  }

  inline std::optional<std::wstring> mode_command(const std::vector<mode_t> &configured, const mode_t &requested) {
    if (!valid_mode(requested)) {
      return std::nullopt;
    }
    std::vector<mode_t> modes;
    std::wstring command = L"SETMODES ";
    const auto append = [&](const mode_t &mode) {
      if (!valid_mode(mode) || std::find(modes.begin(), modes.end(), mode) != modes.end()) {
        return;
      }
      if (!modes.empty()) {
        command += L",";
      }
      modes.push_back(mode);
      command += std::to_wstring(mode.width) + L"x" + std::to_wstring(mode.height) + L"x" + std::to_wstring(mode.fps);
    };
    for (const auto &mode : configured) {
      append(mode);
    }
    append(requested);
    // The driver's command buffer holds 2048 UTF-16 code units including NUL.
    if (command.size() >= 2048) {
      return std::nullopt;
    }
    return command;
  }
}  // namespace platf::vdd
