#include <iostream>
#include <windows.h>
#include <setupapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <direct.h>
#include <conio.h>
#include <iomanip>
#include <sstream>
#include "res_map.h" // Automatically includes resources.h

#pragma comment(lib, "setupapi.lib")

using namespace std;

const GUID GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };

string GetBinPath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    string path(buffer);
    size_t pos = path.find_last_of("\\/");
    return path.substr(0, pos) + "\\bin";
}

void ExtractResources() {
    string binDir = GetBinPath();
    _mkdir(binDir.c_str());

    HMODULE hModule = GetModuleHandle(NULL);
    for (const auto& res : g_resources) {
        string targetPath = binDir + "\\" + res.name;
        
        // Skip if exists
        ifstream f(targetPath.c_str());
        if (f.good()) {
            f.close();
            continue;
        }
        
        HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(res.id), RT_RCDATA);
        if (hRes) {
            HGLOBAL hData = LoadResource(hModule, hRes);
            if (hData) {
                DWORD size = SizeofResource(hModule, hRes);
                void* ptr = LockResource(hData);
                if (ptr && size > 0) {
                    ofstream out(targetPath, ios::binary);
                    out.write((const char*)ptr, size);
                    out.close();
                }
            }
        }
    }
}

void GetAppleState(string& state, int& cpid) {
    state = "NONE";
    cpid = 0;
    
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return;

    bool foundNormal = false;
    SP_DEVINFO_DATA devData;
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    int i = 0;
    while (SetupDiEnumDeviceInfo(hDevInfo, i, &devData)) {
        char instBuf[512];
        DWORD reqSize;
        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devData, instBuf, 512, &reqSize)) {
            string instStr = instBuf;
            for (auto& c : instStr) c = toupper(c);
            
            if (instStr.find("VID_05AC") != string::npos) {
                if (instStr.find("PID_12A8") != string::npos) foundNormal = true;
                else if (instStr.find("PID_1281") != string::npos) state = "RECOVERY";
                else if (instStr.find("PID_1227") != string::npos) state = "DFU";

                size_t cpidPos = instStr.find("CPID:");
                if (cpidPos != string::npos && cpidPos + 9 <= instStr.length()) {
                    string cpidStr = instStr.substr(cpidPos + 5, 4);
                    cpid = stoi(cpidStr, nullptr, 16);
                }
            }
        }
        i++;
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (state == "NONE" && foundNormal) state = "NORMAL";
}

string GetStateString(const string& state) {
    if (state == "NORMAL") return "NORMAL MODE";
    if (state == "RECOVERY") return "RECOVERY MODE";
    if (state == "DFU") return "DFU MODE";
    return "DISCONNECTED";
}

int GetModelType(int cpid) {
    if (cpid == 0) return 0;
    if (cpid < 0x8010) return 1;
    if (cpid == 0x8010) return 2;
    return 3;
}

void RunCommandHidden(string cmd, string dir) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, dir.empty() ? NULL : dir.c_str(), &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

string ExecCmdAndGetOutput(string cmd, string dir) {
    HANDLE hStdOutRead, hStdOutWrite;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, dir.empty() ? NULL : dir.c_str(), &si, &pi);
    CloseHandle(hStdOutWrite);

    DWORD read;
    char buf[4096];
    string output = "";
    while (ReadFile(hStdOutRead, buf, 4095, &read, NULL) && read > 0) {
        buf[read] = 0;
        output += buf;
    }
    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return output;
}

bool EnterRecovery() {
    cout << "[*] Sending command to enter Recovery Mode (via ideviceenterrecovery)..." << endl;
    
    string binDir = GetBinPath();
    string ideviceId = binDir + "\\idevice_id.exe";
    string udid = "";
    
    ifstream f(ideviceId.c_str());
    if (f.good()) {
        f.close();
        string out = ExecCmdAndGetOutput(ideviceId + " -l", binDir);
        if (!out.empty()) {
            size_t end = out.find_first_of("\r\n");
            if (end != string::npos) out = out.substr(0, end);
            udid = out;
        }
    }
    
    string cmd = binDir + "\\ideviceenterrecovery.exe";
    if (!udid.empty()) cmd += " " + udid;
    RunCommandHidden(cmd, binDir);

    cout << "[*] Please wait for the device to reboot into Recovery Mode (cable on screen)..." << endl;
    for (int i = 0; i < 35; i++) {
        string st; int tmp;
        GetAppleState(st, tmp);
        if (st == "RECOVERY") {
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "[+] Device successfully entered Recovery Mode!" << endl;
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
            return true;
        }
        Sleep(1000);
    }
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_INTENSITY);
    cout << "[-] Timeout or command failed. Please try again." << endl;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    return false;
}

void WriteColoredLine(string text, WORD highlightColor = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    size_t i = 0;
    while (i < text.length()) {
        size_t start = text.find('<', i);
        if (start == string::npos) {
            cout << text.substr(i);
            break;
        }
        cout << text.substr(i, start - i);
        size_t end = text.find('>', start);
        if (end == string::npos) {
            cout << text.substr(start);
            break;
        }
        SetConsoleTextAttribute(hConsole, highlightColor);
        cout << text.substr(start + 1, end - start - 1);
        SetConsoleTextAttribute(hConsole, 7);
        i = end + 1;
    }
    cout << string(max(0, (int)(80 - text.length())), ' ') << endl;
}

void DrawUi(int step, string msg, string btn1, string btn2) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = {0, 5};
    SetConsoleCursorPosition(hConsole, pos);
    
    WriteColoredLine(step >= 1 ? "[x] STEP 1: Press and hold <" + btn1 + "> and <" + btn2 + ">" : "[ ] STEP 1: Press and hold <" + btn1 + "> and <" + btn2 + ">");
    cout << string(80, ' ') << endl;
    
    if (step >= 2) {
        WriteColoredLine("[x] STEP 2: RELEASE <" + btn1 + "> IMMEDIATELY! KEEP HOLDING <" + btn2 + ">", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "    " << msg << string(max(0, (int)(76 - msg.length())), ' ') << endl;
    } else {
        WriteColoredLine("[ ] STEP 2: RELEASE <" + btn1 + "> IMMEDIATELY! KEEP HOLDING <" + btn2 + ">");
        cout << string(80, ' ') << endl;
    }
    cout << string(80, ' ') << endl;
    
    WriteColoredLine(step >= 3 ? "[x] STEP 3: Release all buttons when successfully in DFU mode" : "[ ] STEP 3: Release all buttons when successfully in DFU mode", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    cout << string(80, '=') << endl;
}

void RunDfuFlow() {
    string state;
    int cpid;
    GetAppleState(state, cpid);

    if (state == "NORMAL") {
        cout << "[!] Device is in Normal Mode. To determine exact DFU timing, we must enter Recovery first." << endl;
        if (!EnterRecovery()) {
            cout << "Press Enter to return to menu...";
            cin.ignore();
            return;
        }
        GetAppleState(state, cpid);
    }

    int model = GetModelType(cpid);
    if (model == 0) {
        cout << "Could not identify device model. Choose manually:" << endl;
        cout << "1. iPhone 6s and older / iPad with Home button" << endl;
        cout << "2. iPhone 7 / 7 Plus" << endl;
        cout << "3. iPhone 8 / X / 11 / 12... and newer" << endl;
        cout << "-> Select: ";
        string m;
        getline(cin, m);
        if (m == "1") model = 1;
        else if (m == "2") model = 2;
        else model = 3;
    }

    system("cls");
    cout << "======================================================" << endl;
    cout << "                 DFU WIZARD - ENTER DFU               " << endl;
    cout << "======================================================" << endl;
    cout << "PLEASE BE READY AND FOLLOW THE STEPS BELOW:" << endl;
    cout << "------------------------------------------------------" << endl;

    string btn1 = "POWER BUTTON";
    string btn2 = model == 1 ? "HOME BUTTON" : "VOLUME DOWN (-)";

    DrawUi(0, "", btn1, btn2);
    cout << "\nPress ENTER to start...";
    cin.ignore();

    DrawUi(1, "", btn1, btn2);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    string m = "\n>>> PRESS AND HOLD BOTH <" + btn1 + "> AND <" + btn2 + "> RIGHT NOW!!! <<<";
    cout << m << string(max(0, (int)(80 - m.length())), ' ') << endl;
    SetConsoleTextAttribute(hConsole, 7);

    for (int i = 3; i > 0; i--) {
        cout << "\rKeep holding... (" << i << ")" << string(60, ' ');
        Sleep(1000);
    }
    cout << "\rKEEP HOLDING... Powering off device..." << string(40, ' ') << endl;
    
    Sleep(1000);
    string binDir = GetBinPath();
    RunCommandHidden(binDir + "\\irecovery.exe -n", binDir);

    DWORD startPress = GetTickCount();
    while (true) {
        string currState;
        int currCpid;
        GetAppleState(currState, currCpid);
        if (currState != "RECOVERY") break;
        if ((GetTickCount() - startPress) > 10000) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
            cout << "\n[-] Device not responding to reboot command. Please try again." << string(20, ' ') << endl;
            SetConsoleTextAttribute(hConsole, 7);
            cout << "\nPress Enter to return to menu" << string(50, ' ') << endl;
            cin.ignore();
            return;
        }
        Sleep(10);
    }

    cout << '\a'; // Beep
    DrawUi(2, "RELEASE <" + btn1 + ">! Keep holding for about 10 more seconds...", btn1, btn2);
    
    bool success = false;
    int timeout = 10;
    while (timeout > 0) {
        cout << "\rHold <" + btn2 + "> for " << timeout << " more seconds..." << string(40, ' ');
        string st; int tmp;
        GetAppleState(st, tmp);
        if (st == "DFU") {
            success = true;
            break;
        }
        else if (st == "RECOVERY" || st == "NORMAL") {
            break;
        }
        Sleep(1000);
        timeout--;
    }
    
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    int currentY = csbi.dwCursorPosition.Y;
    
    cout << "\nYou can release all buttons! Checking result..." << string(40, ' ') << endl;
    for(int k = 0; k < 5; k++) cout << string(80, ' ') << endl;
    COORD newPos = {0, (SHORT)(currentY + 1)};
    SetConsoleCursorPosition(hConsole, newPos);

    Sleep(2000);
    
    string finalSt; int finalCpid;
    GetAppleState(finalSt, finalCpid);
    if (finalSt == "DFU") success = true;

    if (success) {
        DrawUi(3, "", btn1, btn2);
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "\n[+] SUCCESS! DEVICE IS IN DFU MODE." << string(40, ' ') << endl;
        SetConsoleTextAttribute(hConsole, 7);
    } else {
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout << "\n[-] FAILED! Device rebooted. Please try again from the beginning." << string(20, ' ') << endl;
        SetConsoleTextAttribute(hConsole, 7);
    }
    cout << string(80, ' ') << endl;
    cout << "Press Enter to return to menu" << string(50, ' ') << endl;
    cin.ignore();
}

string RunForceRecoveryGuide() {
    system("cls");
    cout << "===========================================" << endl;
    cout << "  FORCE RECOVERY MODE (Disabled/Locked)    " << endl;
    cout << "===========================================" << endl;
    cout << "\nSince no device is detected, we need to know your iPhone model." << endl;
    cout << "\nSelect your iPhone model:" << endl;
    WriteColoredLine("  1. iPhone 6s and earlier (<HOME BUTTON> models)", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    WriteColoredLine("  2. iPhone 7 / 7 Plus", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    WriteColoredLine("  3. iPhone 8 / X and later (Face ID / no Home Button)", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    cout << "\nPress 1, 2, or 3: ";

    int modelType = 0;
    while (modelType == 0) {
        if (_kbhit()) {
            char k = _getch();
            if (k == '1') { modelType = 1; cout << "1\n"; }
            else if (k == '2') { modelType = 2; cout << "2\n"; }
            else if (k == '3') { modelType = 3; cout << "3\n"; }
        }
        Sleep(50);
    }

    cout << "\n===========================================" << endl;
    cout << "  INSTRUCTIONS: Enter Recovery Mode        " << endl;
    cout << "===========================================" << endl;

    if (modelType == 1) {
        WriteColoredLine("\n  Step 1: Connect the USB cable to your computer.");
        WriteColoredLine("  Step 2: Press and hold <POWER> + <HOME BUTTON> together.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 3: Keep holding for ~10 seconds until the screen goes black.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 4: Release <POWER> but keep holding <HOME BUTTON>.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 5: Wait until the Recovery screen (cable icon) appears.", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    } else if (modelType == 2) {
        WriteColoredLine("\n  Step 1: Connect the USB cable to your computer.");
        WriteColoredLine("  Step 2: Press and hold <POWER> + <VOLUME DOWN> together.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 3: Keep holding for ~10 seconds until the screen goes black.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 4: Release <POWER> but keep holding <VOLUME DOWN>.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 5: Wait until the Recovery screen (cable icon) appears.", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    } else {
        WriteColoredLine("\n  Step 1: Connect the USB cable to your computer.");
        WriteColoredLine("  Step 2: Quick press <VOLUME UP> and release.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 3: Quick press <VOLUME DOWN> and release.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 4: Press and HOLD <POWER BUTTON> (side button).", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        WriteColoredLine("  Step 5: Keep holding until the Recovery screen (cable icon) appears.", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }

    cout << "\n===========================================" << endl;
    cout << "[*] Waiting for device to enter Recovery Mode..." << endl;
    cout << "    (Perform the button combo above now!)" << endl;

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    while (true) {
        string currState; int currCpid;
        GetAppleState(currState, currCpid);
        if (currState == "RECOVERY") {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "\n[+] SUCCESS! Device entered Recovery Mode!" << endl;
            SetConsoleTextAttribute(hConsole, 7);
            return "RECOVERY";
        } else if (currState == "DFU") {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "\n[+] Device entered DFU Mode directly! Even better!" << endl;
            SetConsoleTextAttribute(hConsole, 7);
            return "DFU";
        } else if (currState == "NORMAL") {
            SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
            cout << "\n[*] Device detected in Normal Mode." << endl;
            SetConsoleTextAttribute(hConsole, 7);
            return "NORMAL";
        }
        Sleep(500);
    }
}

int main() {
    ExtractResources();

    bool forceRedraw = true;
    string lastState = "";
    int lastCpid = -1;

    while (true) {
        string state;
        int cpid;
        GetAppleState(state, cpid);

        if (forceRedraw || state != lastState || cpid != lastCpid) {
            forceRedraw = false;
            lastState = state;
            lastCpid = cpid;

            system("cls");
            cout << "========================================" << endl;
            cout << "           DFU HELPER (C++)             " << endl;
            cout << "       github.com/tctvn/DFU-Helper      " << endl;
            cout << "========================================" << endl;
            cout << "Current State: " << GetStateString(state) << endl;
            if (cpid != 0) {
                cout << "Chip ID (CPID): ";
                cout << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << cpid << std::dec << endl;
            }
            cout << "========================================" << endl;

            if (state == "NORMAL") {
                cout << "1. Enter Recovery Mode (Auto)" << endl;
                cout << "2. Enter DFU Mode (Auto via Recovery then countdown)" << endl;
            } else if (state == "RECOVERY") {
                cout << "1. Enter Normal Mode (Exit Recovery)" << endl;
                cout << "2. Enter DFU Mode (Start countdown process)" << endl;
            } else if (state == "DFU") {
                cout << "1. Hard reset instructions (Exit DFU manually)" << endl;
            } else {
                cout << "1. Force enter Recovery Mode (Disabled/Locked device)" << endl;
                cout << "2. Force enter DFU Mode (Recovery then DFU automatically)" << endl;
            }
            cout << "0. Exit program" << endl;
            cout << "========================================" << endl;
            cout << "Press a number (1, 2, or 0) to select an option: ";
        }

        string choice = "";
        DWORD startWait = GetTickCount();
        while ((GetTickCount() - startWait) < 1000) {
            if (_kbhit()) {
                char c = _getch();
                if (c == '0' || c == '1' || c == '2') {
                    cout << c << endl;
                    choice = string(1, c);
                    break;
                }
            }
            Sleep(50);
        }

        if (choice.empty()) continue;
        if (choice == "0") break;

        if (state == "NORMAL") {
            if (choice == "1") {
                EnterRecovery();
                cout << "\nPress Enter to return to menu";
                cin.ignore();
                forceRedraw = true;
            } else if (choice == "2") {
                RunDfuFlow();
                forceRedraw = true;
            }
        } else if (state == "RECOVERY") {
            if (choice == "1") {
                cout << "[*] Sending reset command..." << endl;
                Sleep(1000);
                string binDir = GetBinPath();
                ExecCmdAndGetOutput(binDir + "\\irecovery.exe -n", binDir);
                RunCommandHidden(binDir + "\\irecovery.exe -c reboot", binDir);
                cout << "Check if the device has booted up, then press Enter" << endl;
                cin.ignore();
                forceRedraw = true;
            } else if (choice == "2") {
                RunDfuFlow();
                forceRedraw = true;
            }
        } else if (state == "NONE") {
            if (choice == "1") {
                string result = RunForceRecoveryGuide();
                if (result == "RECOVERY" || result == "NORMAL" || result == "DFU") {
                    cout << "Returning to main menu..." << endl;
                    Sleep(2000);
                }
                forceRedraw = true;
            } else if (choice == "2") {
                string result = RunForceRecoveryGuide();
                if (result == "RECOVERY") {
                    cout << "\n[*] Now transitioning to DFU Mode..." << endl;
                    Sleep(2000);
                    RunDfuFlow();
                } else if (result == "DFU") {
                    cout << "\n[+] Already in DFU Mode! No further action needed." << endl;
                    Sleep(2000);
                } else if (result == "NORMAL") {
                    cout << "\n[*] Device is in Normal Mode. Will enter Recovery first..." << endl;
                    Sleep(1000);
                    EnterRecovery();
                    RunDfuFlow();
                }
                forceRedraw = true;
            }
        } else if (state == "DFU") {
            if (choice == "1") {
                system("cls");
                cout << "===========================================" << endl;
                cout << "   EXIT DFU INSTRUCTIONS (HARD RESET)      " << endl;
                cout << "===========================================" << endl;
                int model = GetModelType(cpid);
                if (model == 1) WriteColoredLine("- Press and hold <POWER BUTTON> + <HOME BUTTON> for 10-15s until Apple logo appears.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                else if (model == 2) WriteColoredLine("- Press and hold <POWER BUTTON> + <VOLUME DOWN> for 10-15s until Apple logo appears.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                else WriteColoredLine("- Quick press <VOLUME UP>, quick press <VOLUME DOWN>, then press and hold <POWER BUTTON> for 10-15s.", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                cout << "===========================================" << endl;
                
                cout << "\n[*] Waiting for device to exit DFU Mode... (Please perform the hard reset)" << endl;
                
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                while (true) {
                    string currState; int currCpid;
                    GetAppleState(currState, currCpid);
                    if (currState != "DFU") {
                        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                        cout << "\n[+] SUCCESS! Device has exited DFU mode and is rebooting." << endl;
                        SetConsoleTextAttribute(hConsole, 7);
                        break;
                    }
                    Sleep(500);
                }
                
                cout << "\n[*] Waiting for device to reconnect..." << endl;
                while (true) {
                    string currState; int currCpid;
                    GetAppleState(currState, currCpid);
                    if (currState == "NORMAL" || currState == "RECOVERY") {
                        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                        cout << "\n[+] Device reconnected in " << currState << " mode!" << endl;
                        SetConsoleTextAttribute(hConsole, 7);
                        break;
                    }
                    Sleep(1000);
                }
                
                cout << "\nReturning to main menu..." << endl;
                Sleep(2000);
                forceRedraw = true;
            }
        }
    }
    return 0;
}
