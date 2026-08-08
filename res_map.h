#pragma once 
#include <vector> 
#include <string> 
#include "resources.h" 
struct ResItem { int id; std::string name; }; 
std::vector<ResItem> g_resources = { 
    { 101, "ideviceenterrecovery.exe" }, 
    { 102, "idevice_id.exe" }, 
    { 103, "irecovery.exe" }, 
    { 104, "libcrypto-3-x64.dll" }, 
    { 105, "libimobiledevice-1.0.dll" }, 
    { 106, "libimobiledevice-glue-1.0.dll" }, 
    { 107, "libirecovery-1.0.dll" }, 
    { 108, "libplist-2.0.dll" }, 
    { 109, "libreadline8.dll" }, 
    { 110, "libssl-3-x64.dll" }, 
    { 111, "libtermcap-0.dll" }, 
    { 112, "libusbmuxd-2.0.dll" }, 
}; 
