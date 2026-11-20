@echo off
setlocal enabledelayedexpansion
title Vaayu Manual Test Publisher

:MENU
cls
echo.
echo =====================================================
echo   Vaayu Manual Test Publisher
echo =====================================================
echo.
echo   1. Simulate HIGH TEMP (27.5C)       - should trigger AC
echo   2. Simulate EMPTY ROOM              - should start shutoff
echo   3. Simulate BATHROOM STEAM (hum 85%%)- exhaust fan on
echo   4. Trigger ALARM (bad sensor reading)
echo   5. Clear alarm / return to normal
echo   6. Simulate HUMAN IR OVERRIDE
echo.
echo   Q. Quit
echo.
set /p CHOICE="Enter choice: "

if /i "%CHOICE%"=="1" goto HIGH_TEMP
if /i "%CHOICE%"=="2" goto EMPTY_ROOM
if /i "%CHOICE%"=="3" goto BATH_STEAM
if /i "%CHOICE%"=="4" goto TRIGGER_ALARM
if /i "%CHOICE%"=="5" goto CLEAR_ALARM
if /i "%CHOICE%"=="6" goto IR_OVERRIDE
if /i "%CHOICE%"=="Q" goto QUIT
echo Invalid choice. Try again.
timeout /t 1 /nobreak >nul
goto MENU

:: ──────────────────────────────────────────────────────────────────
:HIGH_TEMP
echo.
echo [>>] Publishing HIGH TEMP scenario (27.5C, presence, full fan)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/temp"       -m "27.5"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/humidity"   -m "62.0"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/presence"   -m "true"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/distance"   -m "120"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/fan/speed"       -m "100"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason"    -m "MANUAL: High temp 27.5C injected" -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"           -m "NONE"       -r
echo [OK] Published. Dashboard should show 27.5C, fan 100%%.
echo      AC should turn on after 10-min full-fan timer expires.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:EMPTY_ROOM
echo.
echo [>>] Publishing EMPTY ROOM scenario (no presence)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/presence"   -m "false"      -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/distance"   -m "0"          -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/fan/speed"       -m "0"          -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/ac/state"        -m "OFF"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/exhaust/state"   -m "OFF"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason"    -m "MANUAL: Empty room shutoff injected" -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"           -m "NONE"       -r
echo [OK] Published. Dashboard should show no presence, all off.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:BATH_STEAM
echo.
echo [>>] Publishing BATHROOM STEAM scenario (humidity 85%%)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/bathroom/humidity" -m "85.0"     -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/exhaust/state"     -m "ON"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason"      -m "MANUAL: Bathroom steam 85%% - exhaust on" -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"             -m "NONE"     -r
echo [OK] Published. Exhaust fan should show ON on dashboard.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:TRIGGER_ALARM
echo.
echo [>>] Publishing BAD SENSOR READING (temp = 999)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/temp"    -m "999"           -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"        -m "SENSOR_FAULT: temp OOR (999.0C)" -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason" -m "ALARM: Sensor out-of-range! Holding last good value." -r
echo [OK] Published. Dashboard should show ALARM banner.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:CLEAR_ALARM
echo.
echo [>>] Clearing alarm, returning to normal (25C, presence, fan 60%%)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/temp"       -m "25.0"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/humidity"   -m "58.0"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/presence"   -m "true"       -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/room/distance"   -m "150"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/fan/speed"       -m "60"         -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/ac/state"        -m "ON"         -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/ac/setpoint"     -m "26"         -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/exhaust/state"   -m "OFF"        -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/bathroom/humidity" -m "55.0"     -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"           -m "CLEARED"    -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason"    -m "MANUAL: Alarm cleared, normal operation" -r
echo [OK] Published. Alarm should clear on dashboard.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:IR_OVERRIDE
echo.
echo [>>] Publishing HUMAN IR OVERRIDE (pauses automation 30 min)...
mosquitto_pub -h localhost -p 1883 -t "vaayu/cmd/override"  -m "MANUAL"      -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/state/reason"  -m "IR OVERRIDE: human used remote - paused 30 min" -r
mosquitto_pub -h localhost -p 1883 -t "vaayu/alarm"         -m "NONE"        -r
echo [OK] Published. Firmware should freeze automation for 30 minutes.
echo      Dashboard should show MANUAL override mode.
pause
goto MENU

:: ──────────────────────────────────────────────────────────────────
:QUIT
echo.
echo [Vaayu] Manual publisher closed.
endlocal
exit /b 0
