@echo off
set PIO=C:\Users\jffor\.platformio\penv\Scripts\platformio.exe

if "%1"=="fs" (
    echo Flashing filesystem...
    %PIO% run -e dev -t uploadfs
) else if "%1"=="all" (
    echo Flashing code...
    %PIO% run -e dev
    if errorlevel 1 ( echo Code flash failed. & exit /b 1 )
    echo Flashing filesystem...
    %PIO% run -e dev -t uploadfs
) else (
    echo Flashing code...
    %PIO% run -e dev
)
