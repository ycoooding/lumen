/**
 * @file src/platform/windows/vdd.cpp
 * @brief Control transport for the bundled ZakoVDD driver.
 */
#define WIN32_LEAN_AND_MEAN
// SetupAPI requires the base Windows types to be included first.
// clang-format off
#include <Windows.h>
#include <SetupAPI.h>
#include <winioctl.h>
// clang-format on

#include "vdd.h"

#include "src/logging.h"
#include "vdd_mode.h"

#include <array>
#include <boost/property_tree/xml_parser.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace platf::vdd {
  namespace {
    // Must match Common/Include/vdd_control_ioctl.h in ZakoVDD v0.17.5 (Win11) and v0.15.10 (Win10).
    constexpr GUID control_interface {
      0xDA9F8C2B,
      0x7E4F,
      0x49A1,
      {0x9D, 0x4E, 0x6F, 0x2B, 0x0E, 0x1A, 0x0C, 0x4D}
    };
    constexpr DWORD ioctl_command = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA);
    constexpr DWORD ioctl_ping = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS);

    enum class command_result {
      success,
      interface_missing,
      failed,
    };

    std::mutex control_mutex;
    bool monitor_owned = false;

    class device_info_guard {
    public:
      explicit device_info_guard(HDEVINFO handle):
          handle_ {handle} {}

      ~device_info_guard() {
        if (handle_ != INVALID_HANDLE_VALUE) {
          SetupDiDestroyDeviceInfoList(handle_);
        }
      }

      HDEVINFO get() const {
        return handle_;
      }

    private:
      HDEVINFO handle_;
    };

    std::wstring find_control_path() {
      const auto raw = SetupDiGetClassDevsW(
        &control_interface,
        nullptr,
        nullptr,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT
      );
      if (raw == INVALID_HANDLE_VALUE) {
        return {};
      }
      device_info_guard devices {raw};

      SP_DEVICE_INTERFACE_DATA interface_data {};
      interface_data.cbSize = sizeof(interface_data);
      if (!SetupDiEnumDeviceInterfaces(devices.get(), nullptr, &control_interface, 0, &interface_data)) {
        return {};
      }

      DWORD required_size = 0;
      SetupDiGetDeviceInterfaceDetailW(devices.get(), &interface_data, nullptr, 0, &required_size, nullptr);
      if (required_size == 0) {
        return {};
      }

      std::vector<BYTE> buffer(required_size);
      auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data());
      detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
      if (!SetupDiGetDeviceInterfaceDetailW(
            devices.get(),
            &interface_data,
            detail,
            required_size,
            nullptr,
            nullptr
          )) {
        return {};
      }
      return detail->DevicePath;
    }

    command_result send_ioctl_command(const std::wstring &command, DWORD code = ioctl_command) {
      const auto path = find_control_path();
      if (path.empty()) {
        BOOST_LOG(error) << "Lumen virtual display control interface is unavailable. Run the Lumen installer to repair the driver.";
        return command_result::interface_missing;
      }

      const HANDLE device = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION,
        nullptr
      );
      if (device == INVALID_HANDLE_VALUE) {
        BOOST_LOG(error) << "Failed to open the Lumen virtual display driver: " << GetLastError();
        return command_result::failed;
      }

      DWORD returned = 0;
      const DWORD bytes = code == ioctl_ping ? 0 : static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
      const BOOL ok = DeviceIoControl(
        device,
        code,
        bytes ? const_cast<wchar_t *>(command.c_str()) : nullptr,
        bytes,
        nullptr,
        0,
        &returned,
        nullptr
      );
      const DWORD win_error = ok ? ERROR_SUCCESS : GetLastError();
      CloseHandle(device);

      if (!ok) {
        BOOST_LOG(error) << "Lumen virtual display command failed: " << win_error;
        return command_result::failed;
      }
      return command_result::success;
    }

    bool send_command(const std::wstring &command) {
      return send_ioctl_command(command) == command_result::success;
    }

    std::filesystem::path settings_path() {
      std::array<wchar_t, MAX_PATH> module_path {};
      const auto length = GetModuleFileNameW(nullptr, module_path.data(), module_path.size());
      if (length == 0 || length == module_path.size()) {
        return {};
      }
      return std::filesystem::path {module_path.data()}.parent_path() / "config" / "vdd_settings.xml";
    }

    std::vector<mode_t> configured_modes() {
      std::vector<mode_t> modes;
      try {
        std::ifstream input {settings_path()};
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(input, tree);
        std::vector<int> global_rates;
        if (const auto global = tree.get_child_optional("vdd_settings.global")) {
          for (const auto &[name, rate] : *global) {
            if (name == "g_refresh_rate") {
              global_rates.push_back(rate.get_value<int>());
            }
          }
        }
        for (const auto &[name, resolution] : tree.get_child("vdd_settings.resolutions")) {
          if (name != "resolution") {
            continue;
          }
          const int width = resolution.get<int>("width");
          const int height = resolution.get<int>("height");
          for (const int fps : global_rates) {
            modes.push_back({width, height, fps});
          }
          for (const auto &[entry, rate] : resolution) {
            if (entry == "refresh_rate") {
              modes.push_back({width, height, rate.get_value<int>()});
            }
          }
        }
      } catch (const std::exception &err) {
        BOOST_LOG(warning) << "Unable to read virtual display modes: " << err.what();
      }
      if (modes.empty()) {
        modes.push_back({1920, 1080, 60});
      }
      return modes;
    }
  }  // namespace

  bool ready() {
    std::lock_guard lock {control_mutex};
    return send_ioctl_command({}, ioctl_ping) == command_result::success;
  }

  bool mode_available(const std::string &display_name, int width, int height, int fps) {
    if (display_name.empty()) {
      return false;
    }
    const std::wstring name {display_name.begin(), display_name.end()};
    DEVMODEW current {};
    current.dmSize = sizeof(current);
    const bool rotated = EnumDisplaySettingsW(name.c_str(), ENUM_CURRENT_SETTINGS, &current) &&
                         (current.dmFields & DM_DISPLAYORIENTATION) &&
                         (current.dmDisplayOrientation == DMDO_90 || current.dmDisplayOrientation == DMDO_270);
    for (DWORD index = 0; index < 4096; ++index) {
      DEVMODEW mode {};
      mode.dmSize = sizeof(mode);
      if (!EnumDisplaySettingsW(name.c_str(), index, &mode)) {
        break;
      }
      const bool matches = mode.dmPelsWidth == static_cast<DWORD>(width) && mode.dmPelsHeight == static_cast<DWORD>(height);
      const bool rotated_match = rotated && mode.dmPelsWidth == static_cast<DWORD>(height) && mode.dmPelsHeight == static_cast<DWORD>(width);
      if ((matches || rotated_match) && std::abs(static_cast<int>(mode.dmDisplayFrequency) - fps) <= 1) {
        return true;
      }
    }
    return false;
  }

  bool set_mode(int width, int height, int fps) {
    const auto command = mode_command(configured_modes(), {width, height, fps});
    if (!command) {
      BOOST_LOG(error) << "Invalid virtual display mode or mode list exceeds the driver command buffer";
      return false;
    }
    std::lock_guard lock {control_mutex};
    return send_command(*command);
  }

  bool create_monitor() {
    std::lock_guard lock {control_mutex};
    // Enumeration is checked by the caller; a driver restart may have removed an owned monitor.
    if (!send_command(L"CREATEMONITOR")) {
      return false;
    }
    monitor_owned = true;
    BOOST_LOG(info) << "Created the Lumen virtual display";
    return true;
  }

  bool destroy_monitor() {
    std::lock_guard lock {control_mutex};
    if (!monitor_owned) {
      return true;
    }
    if (!send_command(L"DESTROYMONITOR")) {
      return false;
    }
    monitor_owned = false;
    BOOST_LOG(info) << "Destroyed the Lumen virtual display";
    return true;
  }

  bool owns_monitor() {
    std::lock_guard lock {control_mutex};
    return monitor_owned;
  }
}  // namespace platf::vdd
