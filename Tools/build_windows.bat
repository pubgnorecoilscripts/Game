@echo off
setlocal enabledelayedexpansion
rem Builds the editor target and prints the lines that actually explain a failure.
rem
rem Usage:   Tools\build_windows.bat  ["C:\Program Files\Epic Games\UE_5.4"]
rem
rem Do not filter a UE build log on the word "error": UnrealHeaderTool reports
rem plenty of fatal problems without it, for example
rem   Override of UFUNCTION 'X' in parent 'Y' cannot have a UFUNCTION() ...
rem which fails the build while matching no "error" filter at all.

set "PROJECT_DIR=%~dp0.."
set "LOG=%PROJECT_DIR%\build.log"
set "UE=%~1"
if "%UE%"=="" set "UE=C:\Program Files\Epic Games\UE_5.4"

if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" (
	echo Could not find the engine at "%UE%".
	echo Pass the path as the first argument, e.g.
	echo    Tools\build_windows.bat "C:\Program Files\Epic Games\UE_5.4"
	exit /b 1
)

echo Building ParasiteEditor Win64 Development ...
call "%UE%\Engine\Build\BatchFiles\Build.bat" ParasiteEditor Win64 Development -Project="%PROJECT_DIR%\Parasite.uproject" -WaitMutex > "%LOG%" 2>&1
set "RESULT=%ERRORLEVEL%"

echo.
echo ================ problems reported ================
findstr /I /C:"error" /C:": warning" /C:"Override of" /C:"cannot " /C:"undeclared" /C:"unresolved" /C:"fatal" /C:"is not a member" /C:"no matching" /C:"incomplete type" "%LOG%"

echo.
echo ================ last 40 log lines ================
powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG%' -Tail 40"

echo.
if "%RESULT%"=="0" (
	echo BUILD SUCCEEDED.  Open Parasite.uproject and press Play.
) else (
	echo BUILD FAILED with exit code %RESULT%.  Full log: %LOG%
)
exit /b %RESULT%
