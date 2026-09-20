@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Build and flash VVVC. Double-click defaults to COM3 and keeps the result visible.
set "VVVC_PAUSE=0"
if "%~1"=="" set "VVVC_PAUSE=1"
set "VVVC_RESULT=0"
set "VVVC_PORT=COM3"
set "VVVC_BUILD_ONLY=0"

if not "%~2"=="" goto bad_usage
if /i "%~1"=="--help" goto help
if "%~1"=="/?" goto help
if /i "%~1"=="--build-only" (
    set "VVVC_BUILD_ONLY=1"
) else (
    if not "%~1"=="" set "VVVC_PORT=%~1"
)
rem Validate via SET output, without expanding the port value as shell code.
set VVVC_PORT| "%SystemRoot%\System32\findstr.exe" /r /i /x "VVVC_PORT=COM[1-9][0-9]*" >nul
if errorlevel 1 goto bad_usage

set "VVVC_PIO=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
if exist "%VVVC_PIO%" goto found_pio
set "VVVC_PIO="
for /f "delims=" %%P in ('where platformio.exe 2^>nul') do if not defined VVVC_PIO set "VVVC_PIO=%%P"
if defined VVVC_PIO goto found_pio
for /f "delims=" %%P in ('where pio.exe 2^>nul') do if not defined VVVC_PIO set "VVVC_PIO=%%P"
if defined VVVC_PIO goto found_pio
echo ERROR: PlatformIO was not found.
echo Install PlatformIO for VS Code, or put PlatformIO Core on PATH.
set "VVVC_RESULT=1"
goto finish

:found_pio
pushd "%~dp0"
if errorlevel 1 (
    echo ERROR: Cannot open the VVVC project folder.
    set "VVVC_RESULT=1"
    goto finish
)
if "%VVVC_BUILD_ONLY%"=="1" goto build_only
echo Building and flashing VVVC on %VVVC_PORT%.
echo Close serial consoles first. Flashing resets the board and clears RAM phrases.
rem PlatformIO's upload target builds first and does not flash if compilation fails.
"%VVVC_PIO%" run --environment esp32dev --target upload --upload-port "%VVVC_PORT%"
goto result

:build_only
echo Building VVVC without flashing the board.
"%VVVC_PIO%" run --environment esp32dev

:result
set "VVVC_RESULT=%ERRORLEVEL%"
popd
if not "%VVVC_RESULT%"=="0" (
    echo.
    echo ERROR: PlatformIO failed. See the messages above.
    goto finish
)
echo.
if "%VVVC_BUILD_ONLY%"=="1" (
    echo Build succeeded: .pio\build\esp32dev\firmware.bin
) else (
    echo Build and flash succeeded on %VVVC_PORT%.
    echo The board should play its startup tones. Reload learned phrases as needed.
)
goto finish

:bad_usage
echo ERROR: Use a COM port such as COM3, or --build-only.
set "VVVC_RESULT=2"
:help
echo Usage: build_and_flash.bat [COMn ^| --build-only ^| --help]
echo   build_and_flash.bat              Build and flash COM3.
echo   build_and_flash.bat COM7         Build and flash another port.
echo   build_and_flash.bat --build-only Build without uploading.
echo Running with no arguments pauses at the end for double-click use.

:finish
if "%VVVC_PAUSE%"=="1" pause
exit /b %VVVC_RESULT%
