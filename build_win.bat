@echo off

for /f "tokens=3 delims=) " %%a in ('findstr /c:"project(TICK VERSION" CMakeLists.txt') do set VERSION=%%a

echo Configuring CMake for VST3...
cmake -B ./build -DCOPY_AFTER_BUILD="FALSE" -DCMAKE_CXX_FLAGS="/DJUCE_DISPLAY_SPLASH_SCREEN=0" -DCMAKE_POLICY_VERSION_MINIMUM=3.10 -DCMAKE_EXPORT_COMPILE_COMMANDS=1

echo Building VST3 Plugin...
cmake --build ./build --clean-first --target TICK_VST3 --config RelWithDebInfo --parallel

echo Building Windows Installer (NSIS)...
cmake --build ./build --target PACKAGE --config RelWithDebInfo --parallel

echo Build and Packaging Complete! Look in the /build/ folder for TICK-Live-Automation-%VERSION%.exe
pause
