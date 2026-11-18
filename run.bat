@echo off
setlocal
cd /d "%~dp0"

set "GCC="
for /f "delims=" %%G in ('where gcc.exe 2^>nul') do if not defined GCC set "GCC=%%G"

if not defined GCC (
    for /d %%P in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*") do (
        if exist "%%P\mingw64\bin\gcc.exe" set "GCC=%%P\mingw64\bin\gcc.exe"
    )
)

if not defined GCC (
    echo GCC is not installed or could not be found.
    echo Install it with:
    echo winget install --exact --id BrechtSanders.WinLibs.POSIX.UCRT
    pause
    exit /b 1
)

"%GCC%" "church membership system.c" -o "church_membership.exe" -Wall -Wextra -pedantic
if errorlevel 1 (
    echo.
    echo Compilation failed.
    pause
    exit /b 1
)

"%~dp0church_membership.exe"
