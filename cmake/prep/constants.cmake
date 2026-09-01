# source assets will be installed from this directory
set(SUNSHINE_SOURCE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/src_assets")

# Enable the system tray when requested. Platform checks may disable it later.
if(SUNSHINE_ENABLE_TRAY)
    set(SUNSHINE_TRAY 1)
else()
    set(SUNSHINE_TRAY 0)
endif()
