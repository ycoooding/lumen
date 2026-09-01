# NSIS Packaging
# see options at: https://cmake.org/cmake/help/latest/cpack_gen/nsis.html

set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/lumen.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/lumen.ico")

# Add the installer-only activation page without carrying activation checks into Lumen itself.
set(_lumen_nsis_template_dir "${CMAKE_CURRENT_BINARY_DIR}/cpack_templates")
file(MAKE_DIRECTORY "${_lumen_nsis_template_dir}")
file(READ "${CMAKE_ROOT}/Modules/Internal/CPack/NSIS.template.in" _lumen_nsis_template)

set(_lumen_activation_nsh "${CMAKE_SOURCE_DIR}/cmake/packaging/lumen_activation.nsh")
set(_lumen_activation_script "${CMAKE_SOURCE_DIR}/cmake/packaging/lumen_installer_activation.ps1")
cmake_path(NATIVE_PATH _lumen_activation_nsh NORMALIZE _lumen_activation_nsh)
cmake_path(NATIVE_PATH _lumen_activation_script NORMALIZE _lumen_activation_script)

set(_lumen_nsis_include_anchor "  SetCompressor @CPACK_NSIS_COMPRESSOR@")
set(_lumen_nsis_include_replacement
        "${_lumen_nsis_include_anchor}\n\n  !define LUMEN_ACTIVATION_SCRIPT \"${_lumen_activation_script}\"\n  !include \"${_lumen_activation_nsh}\"")
string(FIND "${_lumen_nsis_template}" "${_lumen_nsis_include_anchor}" _lumen_nsis_include_position)
if(_lumen_nsis_include_position EQUAL -1)
    message(FATAL_ERROR "The CPack NSIS template no longer contains the expected compressor anchor.")
endif()
string(REPLACE "${_lumen_nsis_include_anchor}" "${_lumen_nsis_include_replacement}"
        _lumen_nsis_template "${_lumen_nsis_template}")

set(_lumen_nsis_page_anchor "  @CPACK_NSIS_LICENSE_PAGE@\n  Page custom InstallOptionsPage")
set(_lumen_nsis_page_replacement
        "  @CPACK_NSIS_LICENSE_PAGE@\n  Page custom LumenActivationPageCreate LumenActivationPageLeave\n  Page custom InstallOptionsPage")
string(FIND "${_lumen_nsis_template}" "${_lumen_nsis_page_anchor}" _lumen_nsis_page_position)
if(_lumen_nsis_page_position EQUAL -1)
    message(FATAL_ERROR "The CPack NSIS template no longer contains the expected page anchor.")
endif()
string(REPLACE "${_lumen_nsis_page_anchor}" "${_lumen_nsis_page_replacement}"
        _lumen_nsis_template "${_lumen_nsis_template}")

file(WRITE "${_lumen_nsis_template_dir}/NSIS.template.in" "${_lumen_nsis_template}")
set(CPACK_MODULE_PATH "${_lumen_nsis_template_dir};${CMAKE_MODULE_PATH}")

set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
        "${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS}
        StrCmp \$LumenActivationVerified \\\"1\\\" +4
            MessageBox MB_OK|MB_ICONSTOP \\\"未通过安装授权校验，安装已取消。\\\"
            SetErrorLevel 3
            Quit
        ")

# Extra install commands
# Restores permissions on the install directory
# Migrates config files from the root into the new config folder
# Install service
SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}
        nsExec::ExecToLog 'icacls \\\"$INSTDIR\\\" /reset'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\update-path.bat\\\" add'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\migrate-config.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\add-firewall-rule.bat\\\"'
        nsExec::ExecToLog \
          'powershell.exe -NoProfile -ExecutionPolicy Bypass -File \\\"$INSTDIR\\\\scripts\\\\install-gamepad.ps1\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\install-vdd.bat\\\"'
        Pop \$0
        StrCmp \$0 '0' VddInstalled
            MessageBox MB_OK|MB_ICONSTOP 'Lumen 虚拟显示器驱动安装失败，安装无法继续。'
            Abort
        VddInstalled:
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\install-service.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\autostart-service.bat\\\"'
        NoController:
        ")

# Extra uninstall commands
# Uninstall service
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\delete-firewall-rule.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\uninstall-service.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\uninstall-vdd.bat\\\"'
        nsExec::ExecToLog '\\\"$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe\\\" --restore-nvprefs-undo'
        MessageBox MB_YESNO|MB_ICONQUESTION \
            'Do you want to remove Virtual Gamepad?' \
            /SD IDNO IDNO NoGamepad
            nsExec::ExecToLog \
              'powershell.exe -NoProfile -ExecutionPolicy Bypass -File \
                \\\"$INSTDIR\\\\scripts\\\\uninstall-gamepad.ps1\\\"'; \
              skipped if no
        NoGamepad:
        nsExec::ExecToLog '\\\"$INSTDIR\\\\scripts\\\\update-path.bat\\\" remove'
        MessageBox MB_YESNO|MB_ICONQUESTION \
            'Do you want to remove $INSTDIR (this includes the configuration, cover images, and settings)?' \
            /SD IDNO IDNO NoDelete
            RMDir /r \\\"$INSTDIR\\\"; skipped if no
        NoDelete:
        ")

# Adding an option for the start menu
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
# This will be shown on the installed apps Windows settings
set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_PROJECT_NAME}.exe")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "${CPACK_NSIS_CREATE_ICONS_EXTRA}
        SetOutPath '\$INSTDIR'
        CreateShortCut '\$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${CMAKE_PROJECT_NAME}.lnk' \
            '\$INSTDIR\\\\${CMAKE_PROJECT_NAME}.exe' '--shortcut'
        ")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "${CPACK_NSIS_DELETE_ICONS_EXTRA}
        Delete '\$SMPROGRAMS\\\\$MUI_TEMP\\\\${CMAKE_PROJECT_NAME}.lnk'
        ")

# Checking for previous installed versions
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")

set(CPACK_NSIS_URL_INFO_ABOUT "")
set(CPACK_NSIS_CONTACT "")
set(CPACK_NSIS_MANIFEST_DPI_AWARE true)
