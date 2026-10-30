@echo off
rem ============================================================
rem AireRecord build script (MSVC 2017+ / Windows 10 SDK)
rem Builds BOTH:
rem   build\airecordd.exe  - background service (tray, clipboard, DB, IPC)
rem   build\airecord.exe   - UI client
rem ============================================================
setlocal
cd /d "%~dp0"
if not exist build mkdir build
if not exist build\svc_ mkdir build\svc_
if not exist build\cli_ mkdir build\cli_

set "VSROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC"
if not exist "%VSROOT%" set "VSROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC"
if not exist "%VSROOT%" set "VSROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
if not exist "%VSROOT%" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
set "KIT=%ProgramFiles(x86)%\Windows Kits\10"

if not exist "%VSROOT%" (
  echo [ERROR] MSVC not found. Edit VSROOT in this script.
  exit /b 1
)
if not exist "%KIT%" (
  echo [ERROR] Windows 10 SDK not found. Edit KIT in this script.
  exit /b 1
)

set "MSVCVER="
for /d %%v in ("%VSROOT%\*") do set "MSVCVER=%%~nxv"
set "KVER="
for /d %%v in ("%KIT%\Include\10.*") do set "KVER=%%~nxv"

set "PATH=%KIT%\bin\%KVER%\x64;%VSROOT%\%MSVCVER%\bin\HostX64\x64;%PATH%"
set "INCLUDE=%VSROOT%\%MSVCVER%\include;%KIT%\Include\%KVER%\ucrt;%KIT%\Include\%KVER%\um;%KIT%\Include\%KVER%\shared;%KIT%\Include\%KVER%\winrt"
set "LIB=%VSROOT%\%MSVCVER%\lib\x64;%KIT%\Lib\%KVER%\ucrt\x64;%KIT%\Lib\%KVER%\um\x64"

set "CFLAGS=/nologo /W3 /WX- /EHsc /O2 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS"
set "LIBS=user32.lib gdi32.lib shell32.lib comctl32.lib shlwapi.lib advapi32.lib ole32.lib"

echo [1/4] Compiling resources ...
rc /nologo /fo build\app.res res\app.rc
if errorlevel 1 (
  echo [ERROR] Resource compile failed
  exit /b 1
)

echo [2/4] Building service airecordd.exe (MSVC %MSVCVER%, SDK %KVER%) ...
cl %CFLAGS% /I third_party\sqlite ^
   src\service\main.cpp src\core\Database.cpp src\core\MachineConfig.cpp src\ipc\PipeServer.cpp src\ipc\Ipc.cpp ^
   third_party\sqlite\sqlite3.c ^
   build\app.res ^
   /Fe:build\airecordd.exe /Fo:build\svc_\ ^
   /link /SUBSYSTEM:WINDOWS %LIBS%
if errorlevel 1 (
  echo [ERROR] Service build failed
  exit /b 1
)

echo [3/4] Building UI client airecord.exe ...
cl %CFLAGS% ^
   src\main.cpp src\core\App.cpp src\core\SysInfo.cpp src\core\PluginManager.cpp src\core\MachineConfig.cpp ^
   src\plugins\SearchPlugin.cpp src\plugins\ManualEntryPlugin.cpp ^
   src\ipc\PipeClient.cpp src\ipc\Ipc.cpp ^
   build\app.res ^
   /Fe:build\airecord.exe /Fo:build\cli_\ ^
   /link /SUBSYSTEM:WINDOWS %LIBS%
if errorlevel 1 (
  echo [ERROR] Client build failed
  exit /b 1
)

echo [4/4] Build OK: build\airecordd.exe + build\airecord.exe
endlocal
