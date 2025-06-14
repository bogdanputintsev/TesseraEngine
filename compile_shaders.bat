@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Change the current directory to the directory of the script
pushd "%~dp0"

REM Base directory 
set PROJECT_DIR=%CD%

REM Input shaders source code
set SOURCE_SHADERS_DIR=%PROJECT_DIR%\source\shaders

REM Output shaders directory
set SHADERS_OUTPUT_DIR=%PROJECT_DIR%\bin\shaders
set BUILD_DIR=build

REM Error checking
if not defined VULKAN_SDK (
    echo [ERROR] VULKAN_SDK environment variable is not set. Exiting.
    pause
    exit /b 1
)

if not exist "%SOURCE_SHADERS_DIR%" (
    echo [ERROR] Shader source directory "%SOURCE_SHADERS_DIR%" does not exist. Exiting.
    pause
    exit /b 1
)

if not exist "%SHADERS_OUTPUT_DIR%" (
    echo [INFO] Creating directory for shaders output "%SHADERS_OUTPUT_DIR%"...
    mkdir "%SHADERS_OUTPUT_DIR%"
)

REM Compile all shader files
for %%F in ("%SOURCE_SHADERS_DIR%\*.vert" "%SOURCE_SHADERS_DIR%\*.frag" "%SOURCE_SHADERS_DIR%\*.comp" "%SOURCE_SHADERS_DIR%\*.geom" "%SOURCE_SHADERS_DIR%\*.tesc" "%SOURCE_SHADERS_DIR%\*.tese") do (
    set "INPUT_SHADER=%%~fF"
    set "FILENAME=%%~nF"
    set "EXT=%%~xF"
    set "OUTPUT_SHADER=%SHADERS_OUTPUT_DIR%\!FILENAME!!EXT!.spv"

    echo [INFO] Compiling shader file "!INPUT_SHADER!"...
    "%VULKAN_SDK%\Bin\glslc.exe" "!INPUT_SHADER!" -o "!OUTPUT_SHADER!"

    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile "!INPUT_SHADER!". Exiting.
        pause
        exit /b 1
    )
)

echo [INFO] Shaders compiled successfully to "%SHADERS_OUTPUT_DIR%".

REM === Copy bin folder to all subdirectories of build ===
if not exist "%BUILD_DIR%" (
    echo [WARNING] Build directory "%BUILD_DIR%" not found. Skipping bin copy step.
) else (
    for /D %%D in ("%BUILD_DIR%\*") do (
        set "TARGET_BIN=%%D\bin"

        REM Remove old bin folder if it exists
        if exist "!TARGET_BIN!" (
            echo [INFO] Removing existing bin folder in "%%D"...
            rmdir /S /Q "!TARGET_BIN!"
        )

        echo [INFO] Copying bin to "%%D"...
        xcopy /E /I /Q /Y "%PROJECT_DIR%\bin" "!TARGET_BIN!" >nul
    )
)

echo [INFO] Bin folder successfully copied to all build targets.
pause
