@echo off
rem Builds pencil2d and pencil2d_tests with MSVC, then runs the suite. Usage: tools\build-test.cmd ["exact test name" | [tag]]
rem Override VCVARS or QTDIR for another install. Tests run offscreen when the plugin is installed.
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined QTDIR set "QTDIR=C:\Qt\6.8.3\msvc2022_64"
if not exist "%VCVARS%" echo vcvars64.bat not found; set VCVARS& exit /b 1
call "%VCVARS%" >nul 2>&1
set "PATH=%QTDIR%\bin;%PATH%"
rem Offscreen keeps test windows off the desktop.
rem The build folder deploys only the windows plugin, so load platform plugins from QTDIR.
if not defined QT_QPA_PLATFORM if exist "%QTDIR%\plugins\platforms\qoffscreen.dll" set "QT_QPA_PLATFORM=offscreen"& set "QT_QPA_PLATFORM_PLUGIN_PATH=%QTDIR%\plugins\platforms"
cd /d "%~dp0.."
rem Same source guards as CI: no UTF-8 BOM, no ponytail markers, every test file has a TEST_CASE, no private Qt headers outside external/.
powershell -NoProfile -Command "$f = git ls-files --cached --others --exclude-standard -- '*.cpp' '*.h'; $bom = $f | Where-Object { $b = [IO.File]::ReadAllBytes($_); $b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF } | ForEach-Object { \"UTF-8 BOM: $_\" }; $pt = Select-String -Path $f -SimpleMatch 'ponytail:' | ForEach-Object { \"$($_.Path):$($_.LineNumber): use // shortcut:\" }; $et = $f | Where-Object { $_ -like 'tests/src/test_*.cpp' -and -not (Select-String -Path $_ -SimpleMatch 'TEST_CASE' -Quiet) } | ForEach-Object { \"no TEST_CASE: $_\" }; $pv = $f | Where-Object { $_ -notlike 'core_lib/src/external/*' } | ForEach-Object { Select-String -Path $_ -Pattern '#include\s*[<\"\"](qpa/|private/|[^>\"\"]*_p\.h)' } | ForEach-Object { \"$($_.Path):$($_.LineNumber): private Qt header\" }; if ($bom -or $pt -or $et -or $pv) { $bom; $pt; $et; $pv; exit 1 }" || exit /b 1
rem ccache, when installed, shares one compiler cache across worktrees: base_dir makes their paths relative. MSVC needs embedded debug info for it.
set "LAUNCH="
where ccache >nul 2>&1 && set "LAUNCH=-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_POLICY_DEFAULT_CMP0141=NEW -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"
if not defined CCACHE_BASEDIR set "CCACHE_BASEDIR=%CD%"
if not exist build\build.ninja cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DJUSTANIMATE_WERROR=ON "-DCMAKE_PREFIX_PATH=%QTDIR%" %LAUNCH% >nul || exit /b 1
cmake --build build -j2 --target pencil2d pencil2d_tests > build\build.log 2>&1
if errorlevel 1 (findstr /i /c:"error" /c:"warning C" /c:"FAILED:" build\build.log & exit /b 1)
build\pencil2d_tests.exe %* > build\test.log 2>&1
set RC=%errorlevel%
if %RC% neq 0 (findstr /c:"FAILED" /c:".cpp(" build\test.log)
powershell -NoProfile -Command "Get-Content build\test.log -Tail 2"
exit /b %RC%
