@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "BUILD_DIR=build\msvc"
set "LUA_DIR=third_party\lua\src"

if not exist "%LUA_DIR%\lua.h" (
    echo [NeoGLOBAL] Lua sources not found.
    echo Expected: %LUA_DIR%\lua.h
    echo.
    echo Download Lua and place its src directory under:
    echo third_party\lua\src
    exit /b 1
)

rem ------------------------------------------------------------
rem Locate MSVC if this script was not started from a VS shell.
rem ------------------------------------------------------------

where cl >nul 2>nul

if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

    if not exist "!VSWHERE!" (
        echo [NeoGLOBAL] MSVC was not found.
        echo Install Visual Studio or Build Tools with Desktop development with C++.
        exit /b 1
    )

    for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%I"
    )

    if not defined VSINSTALL (
        echo [NeoGLOBAL] A Visual Studio installation with the C++ toolchain was not found.
        exit /b 1
    )

    call "!VSINSTALL!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul

    if errorlevel 1 (
        echo [NeoGLOBAL] Failed to initialize the MSVC environment.
        exit /b 1
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%\lua" mkdir "%BUILD_DIR%\lua"

rem ------------------------------------------------------------
rem Build Lua as a separate static library.
rem lua.c and luac.c are intentionally excluded.
rem ------------------------------------------------------------

set LUA_SOURCES=lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
set "LUA_OBJECTS="

echo [NeoGLOBAL] Building Lua...

for %%F in (%LUA_SOURCES%) do (
    cl /nologo /O2 /std:c11 /D_CRT_SECURE_NO_WARNINGS /I"%LUA_DIR%" /c "%LUA_DIR%\%%F" /Fo"%BUILD_DIR%\lua\%%~nF.obj"

    if errorlevel 1 exit /b 1

    set LUA_OBJECTS=!LUA_OBJECTS! "%BUILD_DIR%\lua\%%~nF.obj"
)

lib /nologo /OUT:"%BUILD_DIR%\lua.lib" !LUA_OBJECTS!

if errorlevel 1 exit /b 1

rem ------------------------------------------------------------
rem Build NeoGLOBAL.
rem neoglobal_unity.c contains the NeoGLOBAL implementation units.
rem ------------------------------------------------------------

echo [NeoGLOBAL] Building NeoGLOBAL...

cl /nologo /O2 /W4 /std:c11 /Iinclude /I"%LUA_DIR%" /c src\main.c /Fo"%BUILD_DIR%\main.obj"
if errorlevel 1 exit /b 1

cl /nologo /O2 /W4 /std:c11 /Iinclude /I"%LUA_DIR%" /c src\neoglobal_unity.c /Fo"%BUILD_DIR%\neoglobal_unity.obj"
if errorlevel 1 exit /b 1

cl /nologo ^
    /Fe:"%BUILD_DIR%\neoglobal.exe" ^
    "%BUILD_DIR%\main.obj" ^
    "%BUILD_DIR%\neoglobal_unity.obj" ^
    "%BUILD_DIR%\lua.lib"

if errorlevel 1 exit /b 1

echo.
echo [NeoGLOBAL] Build successful:
echo %BUILD_DIR%\neoglobal.exe
