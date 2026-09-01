@echo off

rem Get Lumen root directory
for %%I in ("%~dp0\..") do set "OLD_DIR=%%~fI"

rem Create the config directory if it didn't already exist
set "NEW_DIR=%OLD_DIR%\config"
if not exist "%NEW_DIR%\" mkdir "%NEW_DIR%"
icacls "%NEW_DIR%" /reset

rem Migrate Lumen configuration files that aren't already present in the config dir
if exist "%OLD_DIR%\lumen.conf" (
    if not exist "%NEW_DIR%\lumen.conf" (
        move "%OLD_DIR%\lumen.conf" "%NEW_DIR%\lumen.conf"
        icacls "%NEW_DIR%\lumen.conf" /reset
    )
)
if exist "%OLD_DIR%\lumen_state.json" (
    if not exist "%NEW_DIR%\lumen_state.json" (
        move "%OLD_DIR%\lumen_state.json" "%NEW_DIR%\lumen_state.json"
        icacls "%NEW_DIR%\lumen_state.json" /reset
    )
)
