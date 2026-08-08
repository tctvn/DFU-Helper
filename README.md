# DFU Helper

[![Build and Release](https://github.com/tctvn/DFU-Helper/actions/workflows/build.yml/badge.svg)](https://github.com/tctvn/DFU-Helper/actions/workflows/build.yml)

DFU Helper is a standalone, lightweight Windows utility designed to help you interact with Apple devices in Recovery and DFU modes easily. 
It entirely eliminates the need for Python or external dependency installations by embedding all required libraries into a single `.exe` file!

**Author:** [tctvn on GitHub](https://github.com/tctvn)

---

## 🚀 Quick Start (One-Click Run)

You don't need to download or install anything manually! You can run this tool instantly from anywhere.

**Option 1: From Windows Run (Win + R) or CMD**
Press `Win + R`, paste the following command, and hit Enter:
```cmd
powershell -c "irm https://raw.githubusercontent.com/tctvn/DFU-Helper/main/run.ps1 | iex"
```

**Option 2: From PowerShell**
```powershell
irm https://raw.githubusercontent.com/tctvn/DFU-Helper/main/run.ps1 | iex
```

*These commands automatically download the latest compiled version of [`dfu_helper.exe`](https://github.com/tctvn/DFU-Helper/releases/latest/download/dfu_helper.exe) from the Releases page and launch it instantly.*

---

## ✨ Features

- **Single Standalone Executable**: No need to copy the `bin/` folder around. The tool automatically extracts its dependencies to memory/temp on-the-fly.
- **Smart DFU Wizard**: Guides you step-by-step with precision timing based on hardware USB events rather than blind countdowns.
- **Auto-Exit Recovery/DFU**: Fully automated and robust hardware reset commands to kick devices out of frozen states.
- **Real-time Monitoring**: Automatically watches your USB ports and instantly reacts when the device disconnects or changes states.

---

## 🛠️ How to Build Manually

If you want to compile the source code yourself:

1. Clone this repository:
   ```cmd
   git clone https://github.com/tctvn/DFU-Helper.git
   cd DFU-Helper
   ```
2. Double click the `build.bat` script.
   *(This uses the built-in C# compiler `csc.exe` included with Windows .NET Framework. No Visual Studio needed!)*
3. Run the resulting [`dfu_helper.exe`](https://github.com/tctvn/DFU-Helper/releases/latest/download/dfu_helper.exe).

## 📜 License
This project uses open-source binaries from `libimobiledevice` and `irecovery`.
