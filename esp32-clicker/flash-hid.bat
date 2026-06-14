@echo off
set PIO=C:\Users\jffor\.platformio\penv\Scripts\platformio.exe

echo Remember: hold BOOT, tap RESET, release BOOT before each flash step.
echo.

if "%1"=="fs" (
    echo Flashing filesystem...
    %PIO% run -e hid -t uploadfs
) else if "%1"=="all" (
    echo Flashing code... ^(do the BOOT+RESET dance now^)
    %PIO% run -e hid -t upload
    if errorlevel 1 ( echo Code flash failed. & exit /b 1 )
    echo.
    echo Filesystem flash requires a second BOOT+RESET dance.
    echo Do the dance now, then press any key to continue...
    pause >nul
    echo Flashing filesystem...
    %PIO% run -e hid -t uploadfs
) else (
    echo Flashing code...
    %PIO% run -e hid -t upload
)
