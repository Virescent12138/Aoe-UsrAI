@echo off
setlocal
pushd "%~dp0.."

set "PATH=D:\QT5\Tools\mingw530_32\bin;D:\QT5\5.9.2\mingw53_32\bin;%PATH%"

if not exist "Build" mkdir "Build"
pushd "Build"

qmake "..\newAOE.pro" CONFIG+=release
if errorlevel 1 exit /b 1

mingw32-make -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

popd

if not exist "%~dp0..\Build\release\newAOE.exe" (
    echo ERROR: build succeeded but Build\release\newAOE.exe was not generated.
    exit /b 1
)

windeployqt --release "%~dp0..\Build\release\newAOE.exe"

for %%f in (config.json map.njust map1.njust map2.njust map3.njust res.rcc) do (
    if exist "%~dp0..\%%f" copy /y "%~dp0..\%%f" "%~dp0..\Build\release\%%f" >nul
)

start "newAOE" /d "%~dp0..\Build\release" "%~dp0..\Build\release\newAOE.exe"
