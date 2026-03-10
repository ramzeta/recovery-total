@echo off
echo ============================================
echo   Recovery Total - Build Script
echo ============================================
echo.

set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

:: Check for tools
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: g++ not found. Run: pacman -S mingw-w64-x86_64-gcc
    pause
    exit /b 1
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure
echo [1/3] Configuring with CMake + Ninja...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
if %ERRORLEVEL% neq 0 (
    echo Configuration failed!
    pause
    exit /b 1
)

:: Build
echo.
echo [2/3] Building...
ninja
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

:: Copy runtime DLLs
echo.
echo [3/3] Copying runtime DLLs...
copy /Y "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" . >nul
copy /Y "C:\msys64\mingw64\bin\libstdc++-6.dll" . >nul
copy /Y "C:\msys64\mingw64\bin\libwinpthread-1.dll" . >nul

echo.
echo ============================================
echo   BUILD SUCCESSFUL!
echo   Output: build\RecoveryTotal.exe
echo   Run as Administrator for raw disk access
echo ============================================

cd ..
pause
