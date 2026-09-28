# Fetch the exact ZakoVDD and nefcon releases used by Lumen's virtual display.
include_guard(GLOBAL)

set(LUMEN_DRIVER_CACHE "${CMAKE_BINARY_DIR}/_driver_deps")
set(LUMEN_VDD_LATEST_DIR "${LUMEN_DRIVER_CACHE}/vdd-v0.17.5")
set(LUMEN_VDD_WIN10_DIR "${LUMEN_DRIVER_CACHE}/vdd-v0.15.10-win10")
set(LUMEN_NEFCON_DIR "${LUMEN_DRIVER_CACHE}/nefcon-v1.18.74")

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
        "ZakoVDD v0.17.5"
        "https://github.com/qiin2333/zako-vdd/releases/download/v0.17.5/zakovdd.zip"
        "e4177a0c03cb0778e0cf963f552f02c295ab855e0600083f9413446b8c238f6f"
        "${LUMEN_DRIVER_CACHE}/zakovdd-v0.17.5.zip"
        "${LUMEN_VDD_LATEST_DIR}"
        "ZakoVDD.inf")

_lumen_fetch_archive(
        "ZakoVDD v0.15.10 Win10"
        "https://github.com/qiin2333/zako-vdd/releases/download/v0.15.10/zakovdd.zip"
        "b73331f1f319478e4718a03beee76c52883df0f90978f4ff100b15ae36fd0a46"
        "${LUMEN_DRIVER_CACHE}/zakovdd-v0.15.10-win10.zip"
        "${LUMEN_VDD_WIN10_DIR}"
        "ZakoVDD.inf")

_lumen_fetch_archive(
        "nefcon v1.18.74"
        "https://github.com/nefarius/nefcon/releases/download/v1.18.74/nefcon_v1.18.74.zip"
        "625abcdea9e84577d094ab65a8542c9977eb50f2371d216961af01cf4901f172"
        "${LUMEN_DRIVER_CACHE}/nefcon-v1.18.74.zip"
        "${LUMEN_NEFCON_DIR}"
        "x64/nefconw.exe")
