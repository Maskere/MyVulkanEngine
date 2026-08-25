@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

:: Med Ninja ligger den eksekverbare fil direkte i build mappen
set "BUILD_DIR=build"

:: 1. Generer build-filer hvis de mangler (vigtigt for compile_commands.json)
if not exist "%BUILD_DIR%\build.ninja" (
    echo --- Generating Project with Ninja ---
    cmake -G "Ninja" -B %BUILD_DIR% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    :: Lav automatisk hard link til Neovim hvis det mangler
    if not exist "compile_commands.json" (
        mklink /H "compile_commands.json" "%BUILD_DIR%\compile_commands.json"
    )
)

:: 2. Håndter Symlinks til shaders/resources
if not exist "%BUILD_DIR%\shaders" (
    echo Creating Shader Symlink...
    mklink /D "%BUILD_DIR%\shaders" "%cd%\shaders"
)

if not exist "%BUILD_DIR%\assets" (
    echo Creating Assets Symlink...
    mklink /D "%BUILD_DIR%\assets" "%cd%\assets"
)

:: 3. Build projektet
cmake --build %BUILD_DIR% --parallel 8

:: 4. Kør appen hvis build lykkedes
if %ERRORLEVEL% EQU 0 (
    echo --- Launching App ---
    :: Kør fra build mappen
    .\bin\engine.exe
) else (
    echo Build failed!
    pause
)

