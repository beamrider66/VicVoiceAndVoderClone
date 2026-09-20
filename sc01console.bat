@echo off
rem VVVC interactive console launcher.
rem Usage:
rem   sc01console.bat            - open COM3
rem   sc01console.bat COM4       - open a different port
rem   sc01console.bat --list     - list available ports
setlocal
set "SCRIPT_DIR=%~dp0"
set "PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%PYTHON%" (
    echo PlatformIO Python not found at %PYTHON%
    echo Install it with: platformio init or use the system python with pyserial.
    pause
    exit /b 1
)
"%PYTHON%" "%SCRIPT_DIR%tools\console.py" %*
pause
