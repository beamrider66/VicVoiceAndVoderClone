@echo off
cd /d "%~dp0"
echo Open http://localhost:8765 in desktop Chrome or Edge.
echo Leave this window open while flashing. Press Ctrl+C to stop.
python -m http.server 8765 --bind 127.0.0.1
if errorlevel 1 (
  echo Could not start the server. Check Python 3 is installed and port 8765 is free.
  pause
)
