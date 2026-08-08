#pragma once 
#include <vector> 
#include <string> 
#include "resources.h" 
struct ResItem { int id; std::string name; }; 
std::vector<ResItem> g_resources = { 
    { 101, "idevicediagnostics.exe" }, 
    { 102, "ideviceenterrecovery.exe" }, 
    { 103, "idevice_id.exe" }, 
    { 104, "irecovery.exe" }, 
    { 105, "libcrypto-3-x64.dll" }, 
    { 106, "libimobiledevice-1.0.dll" }, 
    { 107, "libimobiledevice-glue-1.0.dll" }, 
    { 108, "libirecovery-1.0.dll" }, 
    { 109, "libplist-2.0.dll" }, 
    { 110, "libreadline8.dll" }, 
    { 111, "libssl-3-x64.dll" }, 
    { 112, "libtermcap-0.dll" }, 
    { 113, "libusbmuxd-2.0.dll" }, 
}; 
