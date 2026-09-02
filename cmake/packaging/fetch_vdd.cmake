# Fetch the exact ZakoVDD and nefcon releases used by Lumen's virtual display.
include_guard(GLOBAL)

set(LUMEN_DRIVER_CACHE "${CMAKE_BINARY_DIR}/_driver_deps")
set(LUMEN_VDD_LATEST_DIR "${LUMEN_DRIVER_CACHE}/vdd-v0.16.2")
set(LUMEN_VDD_WIN10_DIR "${LUMEN_DRIVER_CACHE}/vdd-v0.14.3-win10")
set(LUMEN_NEFCON_DIR "${LUMEN_DRIVER_CACHE}/nefcon-v1.17.40")

function(_lumen_fetch_archive name url sha256 archive destination required_file)
    file(MAKE_DIRECTORY "${LUMEN_DRIVER_CACHE}")

    if(EXISTS "${archive}")
        file(SHA256 "${archive}" cached_sha256)
        if(NOT "${cached_sha256}" STREQUAL "${sha256}")
            file(REMOVE "${archive}")
        endif()
    endif()

    if(NOT EXISTS "${archive}")
        message(STATUS "Downloading ${name}")
        file(DOWNLOAD "${url}" "${archive}"
                EXPECTED_HASH "SHA256=${sha256}"
                TLS_VERIFY ON
                SHOW_PROGRESS
                STATUS download_status)
        list(GET download_status 0 download_code)
        if(NOT download_code EQUAL 0)
            list(GET download_status 1 download_message)
            message(FATAL_ERROR "Unable to download ${name}: ${download_message}")
        endif()
    endif()

    if(NOT EXISTS "${destination}/${required_file}")
        file(REMOVE_RECURSE "${destination}")
        file(MAKE_DIRECTORY "${destination}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${destination}")
    endif()
endfunction()

_lumen_fetch_archive(
        "ZakoVDD v0.16.2"
        "https://github.com/qiin2333/zako-vdd/releases/download/v0.16.2/zakovdd.zip"
        "532833f7e1d14fc716d8db97099a64bf03aa01fafe4058178e89a1d7cbc0f3bd"
        "${LUMEN_DRIVER_CACHE}/zakovdd-v0.16.2.zip"
        "${LUMEN_VDD_LATEST_DIR}"
        "ZakoVDD.inf")

_lumen_fetch_archive(
        "ZakoVDD v0.14.3 Win10"
        "https://github.com/qiin2333/zako-vdd/releases/download/v0.14.3-rc1-edid13-test/ZakoVDD-edid13-issue612.zip"
        "cb6b8eebd1f44d41fcd1d6fa837d74294458b1b564d0a96b17a59e9ad70236ee"
        "${LUMEN_DRIVER_CACHE}/zakovdd-v0.14.3-win10.zip"
        "${LUMEN_VDD_WIN10_DIR}"
        "ZakoVDD.inf")

_lumen_fetch_archive(
        "nefcon v1.17.40"
        "https://github.com/nefarius/nefcon/releases/download/v1.17.40/nefcon_v1.17.40.zip"
        "812bae7ed7dfb7d6d2284bc7de2f8ccebc92ed2a0b1ae893c53b337096e50c1a"
        "${LUMEN_DRIVER_CACHE}/nefcon-v1.17.40.zip"
        "${LUMEN_NEFCON_DIR}"
        "x64/nefconw.exe")
