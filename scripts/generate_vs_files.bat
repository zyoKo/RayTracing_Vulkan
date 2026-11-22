@echo off
rem Generates the Visual Studio solution in build\ from CMakeLists.txt
rem (rtrt.slnx for Visual Studio 2026, rtrt.sln for older versions).
rem   open   also open the solution in Visual Studio afterwards
rem Needs CMake 3.20+ and the Vulkan SDK (VULKAN_SDK).
setlocal
cd /d "%~dp0.."

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found. Install it from https://cmake.org/ and add it to PATH.
    goto Fail
)
if "%VULKAN_SDK%"=="" (
    echo VULKAN_SDK is not set. Install the Vulkan SDK, then open a new terminal.
    goto Fail
)

cmake -S . -B build -A x64
if errorlevel 1 goto Fail

set SOLUTION=build\rtrt.slnx
if not exist "%SOLUTION%" set SOLUTION=build\rtrt.sln
echo.
echo Generated %SOLUTION%
if /i "%~1"=="open" start "" "%SOLUTION%"
goto Done

:Fail
set FAILED=1

:Done
rem Keep the window open when started by double-clicking.
echo %cmdcmdline% | "%SystemRoot%\System32\find.exe" /i "%~0" >nul && pause
if defined FAILED exit /b 1
exit /b 0
