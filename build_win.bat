@echo off
echo Configuring CMake for VST3...
cmake -B ./build -DCOPY_AFTER_BUILD="FALSE" -DCMAKE_CXX_FLAGS="/DJUCE_DISPLAY_SPLASH_SCREEN=0"

echo Building VST3 Plugin...
cmake --build ./build --clean-first --target TICK_VST3 --config RelWithDebInfo

echo Building Windows Installer (NSIS)...
cmake --build ./build --target PACKAGE --config RelWithDebInfo

echo Build and Packaging Complete! Look in the /build/ folder for TICK-Live-Automation-[version].exe
pause
