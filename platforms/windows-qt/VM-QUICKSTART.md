# Windows 11 VM Quick Start

This source bundle builds the x64 Qt version of Solution Finder Enhanced.
Windows on ARM can run the resulting x64 application through emulation, but
this project does not currently produce a native ARM64 executable.

## 1. Extract The Source

Copy the ZIP into the Windows VM and extract it to a short path, such as:

```text
C:\solution-finder-enhanced
```

Avoid building directly inside the ZIP preview or a cloud-synchronized folder.

## 2. Install Core Build Tools

Open PowerShell as Administrator:

```powershell
cd C:\solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\install-prerequisites.ps1
```

This installs CMake, Java 21, and Visual Studio 2022 Build Tools with the C++
workload. The script requires Windows Package Manager (`winget`).

## 3. Install Qt

Download and run the Qt Online Installer:

https://www.qt.io/download-qt-installer-oss

In the component selector, install a Qt 6 release with the
**MSVC 2022 64-bit** component. The default `C:\Qt` installation directory is
recommended.

Restart Windows after the prerequisite and Qt installations finish.

## 4. Build The Application

Open a new PowerShell window:

```powershell
cd C:\solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\build-windows.ps1
```

If Qt was installed outside `C:\Qt`, pass its kit directory:

```powershell
.\build-windows.ps1 -QtRoot "D:\Qt\6.8.3\msvc2022_64"
```

Replace `6.8.3` with the installed Qt version.

## 5. Run The Build

The executable is created at:

```text
platforms\windows-qt\build\Release\solution-finder-enhanced.exe
```

The script also creates a portable test bundle at:

```text
platforms\windows-qt\dist\Solution-Finder-Enhanced-Windows-x64.zip
```

Extract that second ZIP before launching the portable copy.

## Troubleshooting

- **Qt was not found:** Confirm that the installed kit folder ends in
  `msvc2022_64`, or provide it with `-QtRoot`.
- **CMake cannot find Visual Studio:** Restart Windows after running the
  prerequisite script.
- **Java is not recognized:** Restart Windows, then verify `java -version` in a
  new PowerShell window.
- **PowerShell blocks a script:** Run
  `Set-ExecutionPolicy -Scope Process Bypass` in the current window.
