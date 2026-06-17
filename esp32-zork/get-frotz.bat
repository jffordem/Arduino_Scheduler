@echo off
echo Fetching frotz common sources from GitLab...

where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: git not found. Install git and re-run.
    exit /b 1
)

if exist frotz-src (
    echo Removing old frotz-src clone...
    rmdir /S /Q frotz-src
)

git clone --depth 1 https://gitlab.com/DavidGriffith/frotz.git frotz-src
if errorlevel 1 (
    echo ERROR: git clone failed. Check internet connection.
    exit /b 1
)

if not exist lib\frotz\common mkdir lib\frotz\common
xcopy /E /I /Y frotz-src\src\common lib\frotz\common\
if errorlevel 1 (
    echo ERROR: xcopy failed.
    rmdir /S /Q frotz-src
    exit /b 1
)

rmdir /S /Q frotz-src

echo.
echo Done. frotz common sources are in lib\frotz\common\
echo Next steps:
echo   1. flash.bat fs    -- upload data\ to LittleFS (includes index.html)
echo   2. flash.bat       -- compile and flash firmware
echo   3. Open http://zork.local/ in your browser
