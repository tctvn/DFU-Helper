@echo off
setlocal enabledelayedexpansion

echo [*] Generating resources.h, resources.rc, and res_map.h...
echo #ifndef RESOURCES_H > resources.h
echo #define RESOURCES_H >> resources.h
echo // Resource IDs >> resources.h

echo // Auto-generated RC file > resources.rc
echo #include "resources.h" >> resources.rc

echo #pragma once > res_map.h
echo #include ^<vector^> >> res_map.h
echo #include ^<string^> >> res_map.h
echo #include "resources.h" >> res_map.h
echo struct ResItem { int id; std::string name; }; >> res_map.h
echo std::vector^<ResItem^> g_resources = { >> res_map.h

set ID=100
for %%f in (bin\*.*) do (
    set /a ID+=1
    set FILENAME=%%~nxf
    
    :: Remove hyphens and pluses for valid C macro names
    set MACRONAME=!FILENAME:-=_!
    set MACRONAME=!MACRONAME:+=_!
    set MACRONAME=!MACRONAME:.=_!
    
    echo #define RES_!MACRONAME! !ID! >> resources.h
    echo RES_!MACRONAME! RCDATA "bin\\!FILENAME!" >> resources.rc
    echo     { !ID!, "!FILENAME!" }, >> res_map.h
)

echo #endif >> resources.h
echo }; >> res_map.h

echo [*] Compiling with MSVC...
cl.exe /nologo /EHsc /O2 main.cpp resources.rc /link setupapi.lib user32.lib advapi32.lib shell32.lib /OUT:dfu_helper.exe

if %errorlevel% neq 0 (
    echo [-] Compilation failed with MSVC. Trying MinGW g++...
    windres resources.rc -O coff -o resources.res
    g++ -O2 -s -static main.cpp resources.res -o dfu_helper.exe -lsetupapi -luser32 -ladvapi32 -lshell32
    if !errorlevel! neq 0 (
        echo [-] Compilation failed with MinGW too.
        pause
        exit /b 1
    )
)

echo [+] Compilation successful! Output: dfu_helper.exe
pause
