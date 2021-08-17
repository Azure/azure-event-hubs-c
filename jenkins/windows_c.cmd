@REM Copyright (c) Microsoft. All rights reserved.
@REM Licensed under the MIT license. See LICENSE file in the project root for full license information.

setlocal

set build-root=%~dp0..
rem // resolve to fully qualified path
for %%i in ("%build-root%") do set build-root=%%~fi

echo ***checking msbuild***
where /q msbuild
IF ERRORLEVEL 1 (
echo ***setting VC paths***
    IF EXIST "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsMSBuildCmd.bat" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsMSBuildCmd.bat"
)
where msbuild

REM -- C --
cd %build-root%\build_all\windows
call build.cmd --use-websockets --run-unittests --run-e2e-tests %*
if errorlevel 1 goto :eof
cd %build-root%
