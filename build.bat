@echo off
setlocal enabledelayedexpansion

echo ====================================================================
echo  Building FreeCam.asi for Deadly Premonition: The Director's Cut
echo ====================================================================

set ROOT_DIR=%~dp0..
cd /d "%ROOT_DIR%"

if not exist "FreeCam\build" mkdir "FreeCam\build"
if not exist "scripts" mkdir "scripts"

echo [*] Compiling build_freecam.exe...
C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe /nologo /out:build_freecam.exe build_freecam.cs
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile build_freecam.cs!
    exit /b %ERRORLEVEL%
)

echo [*] Building FreeCam with Dear ImGui C++...
build_freecam.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] FreeCam build failed!
    exit /b %ERRORLEVEL%
)

echo ====================================================================
echo  Build and deployment successful!
echo  Deployed: scripts\FreeCam.asi
echo  Deployed: scripts\FreeCam.ini
echo ====================================================================
