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

Then build and run:

```sh
git clone --branch codex/linux-ui https://github.com/rustednuts69/solution-finder-enhanced.git
cd solution-finder-enhanced/platforms/linux-qt
cmake -S . -B build
cmake --build build
./build/solution-finder-enhanced-qt
```

The Qt app should be run from inside the cloned repository for now so it can
find the shared opener book and bundled sfinder files.
