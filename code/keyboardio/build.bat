@echo off
setlocal enabledelayedexpansion

echo ====================================
echo Building keyboardio...
echo ====================================

if not exist build mkdir build

pushd build
cmake ..
if %errorlevel% neq 0 (
    echo CMake generation failed, trying direct GCC build...
    popd
    goto GCC_BUILD
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo CMake build failed, trying direct GCC build...
    popd
    goto GCC_BUILD
)

popd

if exist build\keyboardio.exe (
    echo.
    echo ====================================
    echo Build SUCCESSFUL!
    echo Output: build\keyboardio.exe
    echo ====================================
    goto END
) else if exist build\Release\keyboardio.exe (
    echo.
    echo ====================================
    echo Build SUCCESSFUL!
    echo Output: build\Release\keyboardio.exe
    echo ====================================
    goto END
)

:GCC_BUILD
echo.
echo Attempting direct GCC compilation...
gcc -O2 -I../chuniio main.c ../chuniio/serial_link.c -lsetupapi -luser32 -o keyboardio.exe
if %errorlevel% equ 0 (
    echo.
    echo ====================================
    echo Build SUCCESSFUL!
    echo Output: keyboardio.exe
    echo ====================================
) else (
    echo.
    echo ====================================
    echo Build FAILED!
    echo Please make sure CMake or MinGW GCC is installed and in PATH.
    echo ====================================
)

:END
pause
