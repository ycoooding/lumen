@echo off
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem"
setlocal EnableExtensions EnableDelayedExpansion

set "DRIVER_ROOT=%~dp0driver"
set "DRIVER_DIR=%DRIVER_ROOT%\win10"
set "WIN_BUILD="
set "WIN_BUILD_NUM=0"

if defined VDD_TEST_WIN_BUILD (
    set "WIN_BUILD=%VDD_TEST_WIN_BUILD%"
) else (
    for /f "tokens=3" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /v CurrentBuildNumber 2^>nul ^| find /i "CurrentBuildNumber"') do set "WIN_BUILD=%%A"
)

if defined WIN_BUILD (
    echo(!WIN_BUILD!| findstr /r "^[0-9][0-9]*$" >nul
    if not errorlevel 1 set /a WIN_BUILD_NUM=!WIN_BUILD!
)
if !WIN_BUILD_NUM! GEQ 22000 set "DRIVER_DIR=%DRIVER_ROOT%\latest"
if not exist "%DRIVER_DIR%\ZakoVDD.inf" set "DRIVER_DIR=%DRIVER_ROOT%\latest"

if not exist "%DRIVER_DIR%\ZakoVDD.inf" (
    echo ERROR: Lumen virtual display driver files are missing.
    exit /b 1
)

echo Using Lumen virtual display payload: %DRIVER_DIR%
if /i "%~1"=="--resolve-only" exit /b 0

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DIST_DIR=%ROOT_DIR%\tools\vdd"
set "CONFIG_DIR=%ROOT_DIR%\config"
set "NEFCON=%ROOT_DIR%\tools\nefconw.exe"

if not exist "%NEFCON%" (
    echo ERROR: nefconw.exe is missing.
    exit /b 1
)

if not exist "%CONFIG_DIR%" mkdir "%CONFIG_DIR%"
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
copy /y "%DRIVER_DIR%\*.*" "%DIST_DIR%\" >nul

rem Remove older instances before installing the bundled, pinned driver.
"%NEFCON%" --remove-device-node --hardware-id Root\ZakoVDD --class-guid 4d36e968-e325-11ce-bfc1-08002be10318 >nul 2>&1
"%NEFCON%" --uninstall-driver --inf-path "%DIST_DIR%\ZakoVDD.inf" >nul 2>&1

if not exist "%CONFIG_DIR%\vdd_settings.xml" copy /y "%DIST_DIR%\vdd_settings.xml" "%CONFIG_DIR%\vdd_settings.xml" >nul
reg add "HKLM\SOFTWARE\ZakoTech\ZakoDisplayAdapter" /v VDDPATH /t REG_SZ /d "%CONFIG_DIR%" /f >nul
if errorlevel 1 exit /b 1

certutil -addstore -f root "%DIST_DIR%\ZakoVDD.cer" >nul
if errorlevel 1 exit /b 1

"%NEFCON%" --create-device-node --hardware-id Root\ZakoVDD --service-name ZAKO_HDR_FOR_LUMEN --class-name Display --class-guid 4D36E968-E325-11CE-BFC1-08002BE10318
if errorlevel 1 exit /b 1

"%NEFCON%" --install-driver --inf-path "%DIST_DIR%\ZakoVDD.inf"
if errorlevel 1 exit /b 1

echo Lumen virtual display driver installed.
exit /b 0
