# Solution Finder Enhanced

Native frontends for the enhanced sfinder workflow.

## macOS App

Copy and paste:

```sh
git clone --branch codex/linux-ui https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced
sh macos-native-gui/build_app.sh
open "dist/Solution Finder Enhanced.app"
```

To create a compressed DMG containing the app and an Applications shortcut:

```sh
sh macos-native-gui/build_dmg.sh
open "dist/Solution Finder Enhanced.dmg"
```

Requirements:

- macOS 13 or newer
- Xcode Command Line Tools
- Java runtime

If the Swift tools are missing, run:

```sh
xcode-select --install
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
git clone --branch codex/linux-ui https://github.com/rustednuts69/solution-finder-enhanced.git
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

## Windows Qt App

The Windows app shares its interface and application logic with the Linux Qt
build. It uses a built-in Windows region selector for screenshot import, so it
does not require Flameshot or another screenshot utility.

Install Qt 6, CMake, Visual Studio 2022 Build Tools, and Java. Then open an
**x64 Native Tools Command Prompt for VS 2022** and run:

```bat
git clone --branch codex/windows-ui https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced\platforms\windows-qt
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release build\Release\solution-finder-enhanced.exe
```

Replace `6.8.3` with the installed Qt version. More Windows-specific details are
in `platforms/windows-qt/README.md`.
