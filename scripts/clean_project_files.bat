@echo off
rem Removes generated project files, build output and compiled shaders, leaving only what
rem git tracks (plus imgui.ini and screenshots). Run generate_vs_files.bat afterwards.
setlocal
cd /d "%~dp0.."

tasklist /fi "imagename eq rtrt.exe" | "%SystemRoot%\System32\find.exe" /i "rtrt.exe" >nul
if not errorlevel 1 (
    echo rtrt.exe is running. Close it, then run this again.
    set FAILED=1
    goto Done
)

for %%D in (build out vs .vs src\spv src\shaders\glsl) do call :RemoveDir "%%D"
for /d %%D in (cmake-build-*) do call :RemoveDir "%%D"
for %%F in (src\*.dll src\*.exe src\*.pdb) do call :RemoveFile "%%F"

echo.
echo Clean. Run scripts\generate_vs_files.bat to create the Visual Studio solution.
goto Done

:RemoveDir
if not exist "%~1\" exit /b 0
echo Removing %~1\
rmdir /s /q "%~1" || set FAILED=1
exit /b 0

:RemoveFile
if not exist "%~1" exit /b 0
echo Removing %~1
del /q "%~1" || set FAILED=1
exit /b 0

:Done
rem Keep the window open when started by double-clicking.
echo %cmdcmdline% | "%SystemRoot%\System32\find.exe" /i "%~0" >nul && pause
if defined FAILED exit /b 1
exit /b 0
