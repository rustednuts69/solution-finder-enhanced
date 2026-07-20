# Solution Finder Enhanced Qt UI For Windows

The Windows and Linux apps share the same Qt Widgets interface and application
logic from `platforms/qt-common`. Windows keeps a small build layer for native
packaging and its built-in region screenshot selector.

The current build targets x64. Windows on ARM can run the x64 package through
emulation, but a native ARM64 package is not currently provided.

## Requirements

- 64-bit Windows 10 or Windows 11
- Git
- CMake 3.20 or newer
- Qt 6 with the MSVC 2022 64-bit kit
- Visual Studio 2022 Build Tools with Desktop development with C++
- A Java runtime available on `PATH`

## Build

For a fresh Windows 11 VM, open PowerShell as Administrator and install Git:

```powershell
winget install --id Git.Git --exact --accept-package-agreements --accept-source-agreements
```

Close and reopen PowerShell so Git is available. Clone the repository, enter
the Windows build folder, and run the prerequisite installer:

```powershell
git clone https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\install-prerequisites.ps1
```

The prerequisite script installs CMake, a Java 21 runtime, and Visual Studio
2022 Build Tools with the C++ workload.

Install Qt using the official Qt Online Installer. In its component selector,
choose a Qt 6 version and **MSVC 2022 64-bit**. Restart the VM afterward so the
new command-line tools are available.

After restarting, open PowerShell and return to the cloned build folder:

```powershell
cd solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\build-windows.ps1
```

The script auto-detects Qt under `C:\Qt`, builds the app, runs `windeployqt`, and
creates `dist\Solution-Finder-Enhanced-Windows-x64.zip`. If Qt is installed
somewhere else, specify it explicitly:

```powershell
.\build-windows.ps1 -QtRoot "D:\Qt\6.8.3\msvc2022_64"
```

The equivalent manual build commands are:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

Replace the Qt path with the version installed on your machine. The post-build
step copies the opener book and sfinder runtime beside the executable, so the
build directory can run without referring back to the source tree.

For a manually deployed folder, run Qt's deployment tool after compiling:

```bat
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release --compiler-runtime build\Release\solution-finder-enhanced.exe
```

Run `build\Release\solution-finder-enhanced.exe` to test. A signed installer
is a future packaging step; the application icon and portable runtime bundle
are already included by the current build.

Spin and REN commands and Play scouts are experimental and hidden by default.
Enable them from **Advanced > Enable Experimental Features**.
