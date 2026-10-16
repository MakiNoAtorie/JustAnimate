@echo off
rem Builds pencil2d with MSVC (Debug), then starts the app. Usage: tools\run.bat [project file]
rem Override VCVARS or QTDIR for another install.
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined QTDIR set "QTDIR=C:\Qt\6.8.3\msvc2022_64"
if not exist "%VCVARS%" echo vcvars64.bat not found; set VCVARS& exit /b 1
call "%VCVARS%" >nul 2>&1
set "PATH=%QTDIR%\bin;%PATH%"
cd /d "%~dp0.."
if not exist build\build.ninja cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DJUSTANIMATE_WERROR=ON "-DCMAKE_PREFIX_PATH=%QTDIR%" >nul || exit /b 1
cmake --build build -j2 --target pencil2d > build\build.log 2>&1
if errorlevel 1 (findstr /i /c:"error" /c:"warning C" /c:"FAILED:" build\build.log & exit /b 1)
rem The build folder deploys only the windows plugin; QTDIR supplies the rest.
set "QT_PLUGIN_PATH=%QTDIR%\plugins"
start "" build\justanimate.exe %*
