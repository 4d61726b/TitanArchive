@echo off

pushd %~dp0

if [%1]==[] (set venv=win_build_env) else (set venv=%1)

rmdir /s /q %venv% 2> NUL

python.exe --version > NUL 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Missing Python
    popd
    exit /b 1
)

python.exe -m venv --system-site-packages %venv%
if %errorlevel% neq 0 (
    popd
    exit /b %errorlevel%
)

pushd %venv%

mkdir tasrc
mkdir tasrc\Python
mkdir tasrc\Native

xcopy ..\..\..\Python tasrc\Python /s /e
if %errorlevel% neq 0 goto :exit

xcopy ..\..\..\Native tasrc\Native /s /e
if %errorlevel% neq 0 goto :exit

Scripts\python.exe tasrc\Python\setup.py bdist_wheel --plat-name=win-amd64
if %errorlevel% neq 0 goto :exit
Scripts\python.exe tasrc\Python\setup.py bdist_wheel --plat-name=win32
if %errorlevel% neq 0 goto :exit

copy tasrc\Python\titanarchive\bin\* ..\..\Binaries\
copy dist\* ..\..\Binaries\

:exit
popd
popd
set ret=%errorlevel%
exit /b %ret%
