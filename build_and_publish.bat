@echo off
setlocal
rem Build all supported firmware images, regenerate the guide/site, and publish gh-pages.
pushd "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools\publish_pages.ps1"
set "VVVC_RESULT=%ERRORLEVEL%"
popd
if not "%VVVC_RESULT%"=="0" (
  echo.
  echo Publish failed. See the messages above.
  pause
  exit /b %VVVC_RESULT%
)
echo.
echo Published: https://beamrider66.github.io/VicVoiceAndVoderClone/
pause
