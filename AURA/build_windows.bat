@echo off
REM Prerequis : Visual Studio 2022 (charge "Developpement Desktop C++"), CMake 3.22+, Git
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release --parallel || exit /b 1

set OUT=build\AURA_artefacts\Release\VST3\AURA.vst3
if not exist dist mkdir dist
copy /Y "%OUT%\Contents\x86_64-win\AURA.vst3" dist\AURA.dll
echo.
echo DLL     : dist\AURA.dll
echo Plugin  : %OUT%  (a copier dans C:\Program Files\Common Files\VST3\)
