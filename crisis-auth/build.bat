@echo off
REM Build script for the test DLL and the loader client
REM Requires MSVC (cl) or MinGW (g++) in PATH

set MODULE_SRC=modules\test_dll.cpp
set MODULE_OUT=modules\test.dll

echo [BUILD] Compiling test DLL...

where cl >nul 2>nul
if %errorlevel% == 0 (
    cl /LD /Fe%MODULE_OUT% %MODULE_SRC% /link /EXPORT:Run
    goto :done_module
)

where g++ >nul 2>nul
if %errorlevel% == 0 (
    g++ -shared -o %MODULE_OUT% %MODULE_SRC% -Wl,--export-all-symbols
    goto :done_module
)

echo [BUILD] No compiler found. Install MSVC or MinGW.
exit /b 1

:done_module
echo [BUILD] Module ready: %MODULE_OUT%

REM To build the loader client, integrate these files into your jp-loader project:
REM   client/auth_module.hpp
REM   client/pe_loader.hpp
REM   client/main.cpp

echo [BUILD] Done. Place the generated test.dll in modules\ and run the server.
