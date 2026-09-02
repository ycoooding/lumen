@echo off
set "PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem"
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DIST_DIR=%ROOT_DIR%\tools\vdd"
set "NEFCON=%ROOT_DIR%\tools\nefconw.exe"

if exist "%NEFCON%" (
    "%NEFCON%" --remove-device-node --hardware-id Root\ZakoVDD --class-guid 4d36e968-e325-11ce-bfc1-08002be10318
    if exist "%DIST_DIR%\ZakoVDD.inf" "%NEFCON%" --uninstall-driver --inf-path "%DIST_DIR%\ZakoVDD.inf"
)

reg delete "HKLM\SOFTWARE\ZakoTech\ZakoDisplayAdapter" /f >nul 2>&1
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
echo Lumen virtual display driver removed.
exit /b 0
