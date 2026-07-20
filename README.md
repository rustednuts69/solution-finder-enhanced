# Solution Finder Enhanced

Solution Finder Enhanced combines a native fumen editor, a playable Tetris
board, opener tools, and the bundled sfinder command-line solver in one
cross-platform desktop application.

The shared Qt frontend is the default on macOS, Linux, and Windows. The older
SwiftUI frontend remains available as an alternate macOS build while features
continue to move into the shared application.

## Features

- Native 10x20 fumen editor with colors, minos, pages, mirroring, and import/export
- Opener book, opener detector, and editable opener importer
- Integrated sfinder commands and generated HTML, CSV, text, and fumen previews
- Playable guideline-style Tetris with SRS, configurable controls and tuning
- Queue, hold, screenshot, fumen, undo, ghost-piece, and level progression support
- Guideline-compatible scoring for drops, clears, T-Spins, combos, back-to-back
  clears, and perfect clears
- Automatic perfect-clear scouting with an optional translucent solution preview

Spin and REN scouting are currently experimental. Enable them from
**Advanced > Enable Experimental Features**.

## Repository Layout

- `platforms/qt-common`: shared Qt application used by all supported platforms
- `platforms/linux-qt`: Linux build and AppImage packaging
- `platforms/windows-qt`: Windows build and portable ZIP packaging
- `macos-native-gui`: macOS Qt packaging and preserved SwiftUI frontend
- `macos-native-gui/portable`: shared C game core and tests
- `shared`: bundled opener database
- `solution-finder-1.43`: bundled enhanced sfinder runtime

## macOS App

Copy and paste:

```sh
git clone https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced
sh macos-native-gui/build_app.sh
open "dist/Solution Finder Enhanced.app"
```

The default macOS app uses the shared Qt interface. To build the preserved
Swift frontend instead, run `sh macos-native-gui/build_swift_app.sh`.

To create a compressed DMG containing the app and an Applications shortcut:

```sh
sh macos-native-gui/build_dmg.sh
open "dist/Solution Finder Enhanced.dmg"
```

Requirements:

- macOS 13 or newer
- Qt 6
- CMake
- Xcode Command Line Tools or another C/C++ compiler
- Java runtime

Homebrew can install the Qt build dependencies:

```sh
brew install qt cmake
```

## Linux Qt App

Install dependencies for your distro first.

Debian/Ubuntu:

```sh
sudo apt update
sudo apt install -y git cmake g++ qt6-base-dev default-jre
```

Fedora:

```sh
sudo dnf install -y git cmake gcc-c++ qt6-qtbase-devel java-latest-openjdk
```

Arch/Manjaro:

```sh
sudo pacman -S --needed git cmake gcc qt6-base jre-openjdk
```

For screenshot import/opener detection on Linux, install a region screenshot
tool if your desktop does not already provide one. `flameshot` is recommended;
the app also tries `grim` + `slurp`, `gnome-screenshot`, `spectacle`, `maim`,
and `scrot`.

Then build and run:

```sh
git clone https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced/platforms/linux-qt
cmake -S . -B build
cmake --build build
./build/solution-finder-enhanced-qt
```

To build an AppImage on Linux:

```sh
cd solution-finder-enhanced/platforms/linux-qt
./packaging/build-appimage.sh
```

The AppImage build includes the shared opener book and bundled sfinder files.
Screenshot capture uses an installed desktop region-capture tool. Flameshot is
the recommended option across Linux desktop environments.

## Windows Qt App

The Windows app shares its interface and application logic with the Linux Qt
build. It uses a built-in Windows region selector for screenshot import, so it
does not require Flameshot or another screenshot utility.

The current Windows package targets x64. On a fresh Windows 11 VM, open
PowerShell as Administrator and install Git:

```powershell
winget install --id Git.Git --exact --accept-package-agreements --accept-source-agreements
```

Close and reopen PowerShell so Git is available, then clone the project and
install the remaining prerequisites:

```powershell
git clone https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\install-prerequisites.ps1
```

Install Qt 6 with the **MSVC 2022 64-bit** component, restart Windows, and run:

```powershell
cd solution-finder-enhanced\platforms\windows-qt
Set-ExecutionPolicy -Scope Process Bypass
.\build-windows.ps1
```

The build creates
`platforms\windows-qt\dist\Solution-Finder-Enhanced-Windows-x64.zip`.
More details and manual commands are in `platforms/windows-qt/README.md`.

## Tests

The Qt build runs the portable C game-core tests when using the macOS packaging
script. They can also be run directly after a CMake build:

```sh
ctest --test-dir build --output-on-failure
```

The test suite covers piece orientation and rotation, gravity, lock reset,
perfect-clear scouting, and scoring.
