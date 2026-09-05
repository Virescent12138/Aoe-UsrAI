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
