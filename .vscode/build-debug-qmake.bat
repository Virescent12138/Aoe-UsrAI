@echo off
setlocal
pushd "%~dp0.."

set "PATH=D:\QT5\Tools\mingw530_32\bin;D:\QT5\5.9.2\mingw53_32\bin;%PATH%"

if not exist "Debug" mkdir "Debug"
pushd "Debug"

qmake "..\newAOE.pro" CONFIG+=debug
if errorlevel 1 exit /b 1

mingw32-make -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

popd

if not exist "%~dp0..\Debug\debug\newAOE.exe" (
    echo ERROR: build succeeded but Debug\debug\newAOE.exe was not generated.
    exit /b 1
)

windeployqt --debug "%~dp0..\Debug\debug\newAOE.exe"
