@ECHO OFF

if "%~1"=="" (
	echo "build.bat <MODE>: MODE==0: incremental build, MODE==1: clean build, MODE==2: build releaseable package"
	exit /b 1
)

REM set PRT="-Dprt_DIR=%~dp0\..\prt\com.esri\com.esri.prt.build\cmake"
set PRT=

set MODE=%~1
if "%MODE%"=="0" (
	set P4HENC_TARGET=install
	set P4H_TARGET=install
) else (
if "%MODE%"=="1" (
	set P4HENC_TARGET=clean install
	set P4H_TARGET=clean install
) else (
if "%MODE%"=="2" (
	set P4HENC_TARGET=clean install
	set P4H_TARGET=clean install package
) ) )

set GEN="NMake Makefiles"

setlocal
pushd codec
call "%ProgramFiles(x86)%\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" amd64
if NOT "%MODE%"=="0" ( rd /S /Q build install )
mkdir build
pushd build
set BT=Release
REM set BT=Debug
cmake -G %GEN% %PRT% -DCMAKE_BUILD_TYPE=%BT% ../src
nmake %P4HENC_TARGET%
popd
popd
endlocal

set BT=RelWithDebInfo
REM set BT=Debug

setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" amd64
REM set MSVCDir=%VCINSTALLDIR%
set MSVCDir=C:/Program Files (x86)/Microsoft Visual Studio 11.0/VC

pushd client
if NOT "%MODE%"=="0" ( rd /S /Q build install )
mkdir build
pushd build
cmake -G %GEN% %PRT% -DCMAKE_BUILD_TYPE=%BT% ../src 
nmake  %P4H_TARGET%
popd
popd

endlocal
