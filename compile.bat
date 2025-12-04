@echo off
setlocal
cd /d "%~dp0"

if exist "build" (
  echo Cleaning previous build cache...
  rmdir /s /q "build" 2>nul
)

echo Configuring...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
if errorlevel 1 goto :err

echo Building...
cmake --build build --config Release --parallel
if errorlevel 1 goto :err

echo Done.
exit /b 0

:err
echo Build failed. See messages above.
exit /b 1
