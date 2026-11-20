@echo off
setlocal
title Vaayu Hub - Testing Environment

echo =====================================================
echo   Vaayu Hub Testing Environment Launcher
echo =====================================================
echo.

echo [1/3] Starting Local MQTT and WebSocket Broker...
start "Vaayu - Broker" cmd /k "cd /d C:\D FOLDER\Projects\vaayu\testing && python broker.py"

echo [2/3] Waiting 3 seconds for broker...
timeout /t 3 /nobreak >nul

echo [3/3] Starting Room Sensor Simulator...
start "Vaayu - Simulator" cmd /k "cd /d C:\D FOLDER\Projects\vaayu\testing && python simulate.py"

echo Starting Phone Dashboard...
start "Vaayu - Dashboard" cmd /k "cd /d C:\D FOLDER\Projects\vaayu\dashboard && npx expo start --web"

echo.
echo =====================================================
echo   All 3 testing tools are now running!
echo   A browser tab will open automatically.
echo   Press any key to close this launcher.
echo =====================================================
pause
