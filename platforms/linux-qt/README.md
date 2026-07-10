# Solution Finder Enhanced Qt UI

This is the first native Qt shell for the future Linux build of Solution Finder
Enhanced. It is intentionally separate from the macOS SwiftUI app so Linux work
can move forward without disturbing the working macOS version.

## Current Scope

- Native Qt Widgets layout
- 10x20 editable board with click and drag painting
- Opener group and variation selector from `shared/openers.json`
- Native fumen page controls, mino placement controls, and fumen output code
- Core sfinder command controls
- Process runner for the bundled `solution-finder-1.43/sfinder.jar`
- Command output pane

Screenshot import, playable mode, output preview, and the full advanced settings
set are still expected follow-up work in this Qt port.

## Build And Run On Linux

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

If you already cloned the repository, run:

```sh
cd platforms/linux-qt
cmake -S . -B build
cmake --build build
./build/solution-finder-enhanced-qt
```

The app discovers the repository root by looking for `shared/openers.json`, so
run it from inside a clone of `solution-finder-enhanced` while this port is
early.
