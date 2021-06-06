@echo off

pushd %~dp0

call buildall.bat win_test_env
if %errorlevel% neq 0 (
    popd
    exit /b %errorlevel%
)

pushd win_test_env

Scripts\python.exe -m pip install --no-index --find-links=..\..\Binaries titanarchive
if %errorlevel% neq 0 goto :exit
Scripts\python.exe -m unittest discover tasrc\Python\tests -vv
if %errorlevel% neq 0 goto :exit

:exit
popd
popd
set ret=%errorlevel%
if not %ret% == 0 echo Failed with exit code %ret%
exit /b %ret%
