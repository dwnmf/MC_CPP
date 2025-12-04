@echo off
setlocal
cd /d "%~dp0"

:: Блок очистки УБРАН. Теперь сборка будет инкрементальной.

echo Configuring...
:: CMake сам поймет, если папка build уже есть, и просто обновит настройки, если нужно.
cmake -G "MinGW Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
if errorlevel 1 goto :err

echo Building...
cmake --build build --parallel
if errorlevel 1 goto :err

echo Done.
exit /b 0

:err
echo Build failed. See messages above.
exit /b 1