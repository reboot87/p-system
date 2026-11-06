@echo off
rem ============================================================
rem AireRecord build script -- CMake + Visual Studio 2017 (MSVC)
rem
rem Only supported toolchain: CMake + Visual Studio 2017 (MSVC).
rem Do NOT build this project with MinGW / g++.
rem
rem Single build entry, everything goes through CMake:
rem   build-cmake\Release\airecordd.exe     - background service
rem   build-cmake\Release\airecord.exe      - UI client
rem   build-cmake\Release\airui_demo.exe    - AirUI reuse demo
rem   build-cmake\Release\airui_gallery.exe - component gallery
rem
rem Usage:
rem   build.bat              incremental Release build
rem   build.bat clean        wipe build dir and reconfigure
rem   build.bat test         build then run ctest
rem   build.bat clean test   clean + build + test
rem ============================================================
setlocal
cd /d "%~dp0"

set "BUILD_DIR=build-cmake"
set "CONFIG=Release"
set "ARG1=%~1"
set "ARG2=%~2"
if /i "%ARG1%"=="clean" set "DO_CLEAN=1"
if /i "%ARG2%"=="clean" set "DO_CLEAN=1"
if /i "%ARG1%"=="test"  set "DO_TEST=1"
if /i "%ARG2%"=="test"  set "DO_TEST=1"

rem ---- locate CMake ----
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
  if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
  ) else (
    echo [ERROR] CMake not found. Install CMake 3.15+ and add to PATH.
    exit /b 1
  )
)

rem ---- clean: wipe build dir ----
if defined DO_CLEAN (
  echo [clean] removing %BUILD_DIR% ...
  if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

rem ---- configure: VS2017 is required; 2019/2022 only as fallback ----
if exist %BUILD_DIR%\CMakeCache.txt (
  echo [1/3] Configure: reusing existing %BUILD_DIR%
) else (
  echo [1/3] Configuring first time ...
  call :try_generator "Visual Studio 15 2017"
  if not defined CONFIGURED call :try_generator "Visual Studio 16 2019"
  if not defined CONFIGURED call :try_generator "Visual Studio 17 2022"
  if not defined CONFIGURED (
    echo [ERROR] Visual Studio 2017 / 2019 / 2022 with C++ tools not found.
    exit /b 1
  )
)

rem ---- build ----
echo [2/3] Building %CONFIG% ...
"%CMAKE%" --build %BUILD_DIR% --config %CONFIG%
if errorlevel 1 (
  echo [ERROR] Build failed
  exit /b 1
)

rem ---- optional tests ----
if defined DO_TEST (
  echo [3/3] Running ctest ...
  pushd %BUILD_DIR%
  ctest -C %CONFIG% --output-on-failure
  if errorlevel 1 (
    popd
    echo [ERROR] Tests failed
    exit /b 1
  )
  popd
) else (
  echo [3/3] Skipped tests - run "build.bat test"
)

echo.
echo Build OK:
echo   %CD%\%BUILD_DIR%\%CONFIG%\airecordd.exe
echo   %CD%\%BUILD_DIR%\%CONFIG%\airecord.exe
echo   %CD%\%BUILD_DIR%\%CONFIG%\airui_demo.exe
echo   %CD%\%BUILD_DIR%\%CONFIG%\airui_gallery.exe
endlocal
exit /b 0

:try_generator
echo        trying generator: %~1
"%CMAKE%" -S . -B %BUILD_DIR% -G "%~1" -A x64
if errorlevel 1 (
  if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
  exit /b 1
)
set "CONFIGURED=1"
exit /b 0
