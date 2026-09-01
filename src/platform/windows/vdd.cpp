/**
 * @file src/platform/windows/vdd.cpp
 * @brief Control transport for the bundled ZakoVDD driver.
 */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <winioctl.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "vdd.h"

#include "src/logging.h"

namespace platf::vdd {
  namespace {
    // Must match Common/Include/vdd_control_ioctl.h in ZakoVDD v0.16.2.
    constexpr GUID control_interface {
      0xDA9F8C2B, 0x7E4F, 0x49A1, {0x9D, 0x4E, 0x6F, 0x2B, 0x0E, 0x1A, 0x0C, 0x4D}
    };
    constexpr DWORD ioctl_command = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA);
    constexpr auto legacy_pipe = L"\\\\.\\pipe\\ZakoVDDPipe";

    enum class command_result {
      success,
      interface_missing,
      failed,
    };

    std::mutex control_mutex;
    bool monitor_owned = false;

    class device_info_guard {
    public:
      explicit device_info_guard(HDEVINFO handle): handle_ {handle} {}
      ~device_info_guard() {
        if (handle_ != INVALID_HANDLE_VALUE) {
          SetupDiDestroyDeviceInfoList(handle_);
        }
      }

      HDEVINFO get() const { return handle_; }

    private:
      HDEVINFO handle_;
    };

    std::wstring find_control_path() {
      const auto raw = SetupDiGetClassDevsW(
        &control_interface, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
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
            devices.get(), &interface_data, detail, required_size, nullptr, nullptr)) {
        return {};
      }
      return detail->DevicePath;
    }

    command_result send_ioctl_command(const std::wstring &command) {
      const auto path = find_control_path();
      if (path.empty()) {
        return command_result::interface_missing;
      }

      const HANDLE device = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
      if (device == INVALID_HANDLE_VALUE) {
        BOOST_LOG(error) << "Failed to open the Lumen virtual display driver: " << GetLastError();
        return command_result::failed;
      }

      DWORD returned = 0;
      const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
      const BOOL ok = DeviceIoControl(
        device, ioctl_command, const_cast<wchar_t *>(command.c_str()), bytes,
        nullptr, 0, &returned, nullptr);
      const DWORD win_error = ok ? ERROR_SUCCESS : GetLastError();
      CloseHandle(device);

      if (!ok) {
        BOOST_LOG(error) << "Lumen virtual display command failed: " << win_error;
        return command_result::failed;
      }
      return command_result::success;
    }

    bool send_pipe_command(const std::wstring &command) {
      for (int attempt = 0; attempt < 10; ++attempt) {
        const HANDLE pipe = CreateFileW(
          legacy_pipe, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
          OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
          WaitNamedPipeW(legacy_pipe, 200);
          Sleep(200);
          continue;
        }

        OVERLAPPED overlapped {};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) {
          CloseHandle(pipe);
          return false;
        }
        const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        DWORD written = 0;
        BOOL ok = WriteFile(pipe, command.c_str(), bytes, &written, &overlapped);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
          ok = WaitForSingleObject(overlapped.hEvent, 3000) == WAIT_OBJECT_0 &&
               GetOverlappedResult(pipe, &overlapped, &written, FALSE);
        }
        if (!ok) {
          CancelIo(pipe);
        } else {
          // The old driver executes the command on its pipe thread immediately after reading it.
          Sleep(100);
        }
        CloseHandle(overlapped.hEvent);
        CloseHandle(pipe);
        return ok;
      }

      BOOST_LOG(error) << "Lumen virtual display legacy control pipe was not found";
      return false;
    }

    bool send_command(const std::wstring &command) {
      switch (send_ioctl_command(command)) {
        case command_result::success:
          return true;
        case command_result::failed:
          return false;
        case command_result::interface_missing:
          return send_pipe_command(command);
      }
      return false;
    }

    std::filesystem::path legacy_settings_path() {
      std::array<wchar_t, MAX_PATH> module_path {};
      const auto length = GetModuleFileNameW(nullptr, module_path.data(), module_path.size());
      if (length == 0 || length == module_path.size()) {
        return {};
      }
      return std::filesystem::path {module_path.data()}.parent_path() / "config" / "vdd_settings.xml";
    }

    bool update_legacy_mode(int width, int height, int fps) {
      const auto settings_path = legacy_settings_path();
      std::ifstream input {settings_path, std::ios::binary};
      if (!input) {
        BOOST_LOG(error) << "Unable to open the Lumen virtual display settings: " << settings_path.string();
        return false;
      }

      std::string xml {std::istreambuf_iterator<char> {input}, std::istreambuf_iterator<char> {}};
      bool changed = false;

      const std::regex refresh_pattern {
        "<g_refresh_rate>\\s*" + std::to_string(fps) + "\\s*</g_refresh_rate>"
      };
      if (!std::regex_search(xml, refresh_pattern)) {
        const auto global_end = xml.find("</global>");
        if (global_end == std::string::npos) {
          return false;
        }
        xml.insert(global_end, "        <g_refresh_rate>" + std::to_string(fps) + "</g_refresh_rate>\r\n    ");
        changed = true;
      }

      const std::regex resolution_pattern {
        "<resolution>\\s*<width>\\s*" + std::to_string(width) +
        "\\s*</width>\\s*<height>\\s*" + std::to_string(height) + "\\s*</height>"
      };
      if (!std::regex_search(xml, resolution_pattern)) {
        const auto resolutions_end = xml.find("</resolutions>");
        if (resolutions_end == std::string::npos) {
          return false;
        }
        std::ostringstream mode;
        mode << "        <resolution>\r\n"
             << "            <width>" << width << "</width>\r\n"
             << "            <height>" << height << "</height>\r\n"
             << "            <refresh_rate>" << fps << "</refresh_rate>\r\n"
             << "        </resolution>\r\n    ";
        xml.insert(resolutions_end, mode.str());
        changed = true;
      }

      if (!changed) {
        return true;
      }

      auto temporary_path = settings_path;
      temporary_path += ".lumen.tmp";
      {
        std::ofstream output {temporary_path, std::ios::binary | std::ios::trunc};
        output.write(xml.data(), static_cast<std::streamsize>(xml.size()));
        if (!output) {
          return false;
        }
      }
      if (!MoveFileExW(temporary_path.c_str(), settings_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary_path);
        return false;
      }
      return true;
    }
  }  // namespace

  bool set_mode(int width, int height, int fps) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || fps <= 0 || fps > 1000) {
      BOOST_LOG(warning) << "Ignoring invalid virtual display mode: " << width << 'x' << height << '@' << fps;
      return false;
    }

    std::wostringstream command;
    command << L"SETMODES " << width << L'x' << height << L'x' << fps;
    std::lock_guard lock {control_mutex};
    switch (send_ioctl_command(command.str())) {
      case command_result::success:
        return true;
      case command_result::failed:
        return false;
      case command_result::interface_missing:
        if (!update_legacy_mode(width, height, fps)) {
          return false;
        }
        return send_pipe_command(L"RELOAD_DRIVER");
    }
    return false;
  }

  bool create_monitor() {
    std::lock_guard lock {control_mutex};
    if (monitor_owned) {
      return true;
    }
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
