@echo off
setlocal
pushd "%~dp0.."

call "D:\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 (
    echo ERROR: vcvarsall.bat failed.
    exit /b 1
)

"D:\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset release
if errorlevel 1 exit /b 1

"D:\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build --preset release --parallel
if errorlevel 1 exit /b 1

if not exist "%~dp0..\Build\bin\aoe.exe" (
    echo ERROR: build succeeded but Build\bin\aoe.exe was not generated.
    exit /b 1
)

set "QT_BIN=D:\QT5\5.9.2\msvc2017_64\bin"
set "PATH=%QT_BIN%;%PATH%"
"%QT_BIN%\windeployqt.exe" --release --no-compiler-runtime "%~dp0..\Build\bin\aoe.exe"
if errorlevel 1 exit /b 1

for %%f in (config.json map.njust map1.njust map2.njust map3.njust res.rcc) do (
    if exist "%~dp0..\%%f" copy /y "%~dp0..\%%f" "%~dp0..\Build\bin\%%f" >nul
)
