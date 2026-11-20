@echo off
title Vaayu Hub - Stop Testing Environment
echo.
echo [Vaayu] Stopping all testing processes...

:: Kill Mosquitto
taskkill /F /IM mosquitto.exe >nul 2>&1
if errorlevel 1 (
    echo   [skip] mosquitto.exe was not running.
) else (
    echo   [OK]   mosquitto.exe stopped.
)

:: Kill Python simulate.py and broker.py
wmic process where "name='python.exe' and (commandline like '%%simulate.py%%' or commandline like '%%broker.py%%')" delete >nul 2>&1
if errorlevel 1 (
    echo   [skip] No simulate.py or broker.py Python process found.
) else (
    echo   [OK]   Python testing processes stopped.
)


:: Also kill any stray pythonw.exe for good measure
taskkill /F /IM pythonw.exe >nul 2>&1

echo.
echo [Vaayu] All processes stopped.
echo.
pause
