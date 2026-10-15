@echo off
rem Builds pencil2d and pencil2d_tests with MSVC, then runs the suite. Usage: tools\build-test.cmd [catch test filter]
rem Override VCVARS or QTDIR for another install. The offscreen Qt plugin is not in every install, so the tests use the default platform.
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined QTDIR set "QTDIR=C:\Qt\6.8.3\msvc2022_64"
if not exist "%VCVARS%" echo vcvars64.bat not found; set VCVARS& exit /b 1
call "%VCVARS%" >nul 2>&1
set "PATH=%QTDIR%\bin;%PATH%"
cd /d "%~dp0.."
if not exist build\build.ninja cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DJUSTANIMATE_WERROR=ON "-DCMAKE_PREFIX_PATH=%QTDIR%" >nul || exit /b 1
cmake --build build -j2 --target pencil2d pencil2d_tests > build\build.log 2>&1
if errorlevel 1 (findstr /i /c:"error" /c:"warning C" /c:"FAILED:" build\build.log & exit /b 1)
build\pencil2d_tests.exe %* > build\test.log 2>&1
set RC=%errorlevel%
if %RC% neq 0 (findstr /c:"FAILED" /c:".cpp(" build\test.log)
powershell -NoProfile -Command "Get-Content build\test.log -Tail 2"
exit /b %RC%
