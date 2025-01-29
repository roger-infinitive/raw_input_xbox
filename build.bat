@echo off

setlocal

REM Configuration

REM Vars
set BUILD_DIR=build
set EDITOR_MODE_FLAG=0
set TRACE_FLAG=0
set PACKAGE_FILES=0
set BUILD_FLAGS=/MT /O2 /GL /Ob2 /Oy
set PDB_PATH=
set LIB_PATH=libraries/x64
set OUTPUT_NAME=%BUILD_DIR%/Raw Input Example.exe
set DEBUG_SET= 0

REM Check arguments
for %%a in (%*) do (
    if "%%a"=="Debug" (
        set BUILD_FLAGS=/Od /Zi /MTd /D "_DEBUG"
        set PDB_PATH=%BUILD_DIR%\vc140.pdb
        set LIB_PATH=libraries/debug/x64
        set OUTPUT_NAME=%BUILD_DIR%\Raw Input Example DEBUG.exe
        set DEBUG_SET=1
        echo Using DEBUG build
    )
)

REM Create the build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Compile main.c
cl %BUILD_FLAGS% /I "include" /Fd"%PDB_PATH%" /Fo:%BUILD_DIR%\ /Fe:"%OUTPUT_NAME%" main.c ^
/link /LIBPATH:"%LIB_PATH%" user32.lib gdi32.lib hid.lib ^
/subsystem:windows

@echo off

REM Check for compile errors
if errorlevel 1 (
	powershell -Command "Write-Host 'Compilation failed!' -ForegroundColor Red"
	exit /b 1
)

if %DEBUG_SET% == 1 (
    powershell -Command "Write-Host 'Debug build complete!' -ForegroundColor Green"
) else (
	powershell -Command "Write-Host 'Release build complete!' -ForegroundColor Green"
)

endlocal