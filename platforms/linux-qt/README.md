# Solution Finder Enhanced Qt UI

This is the first native Qt shell for the future Linux build of Solution Finder
Enhanced. It is intentionally separate from the macOS SwiftUI app so Linux work
can move forward without disturbing the working macOS version.

## Current Scope

- Native Qt Widgets layout
- 10x20 editable board with click and drag painting
- Opener group and variation selector from `shared/openers.json`
- Core sfinder command controls
- Process runner for the bundled `solution-finder-1.43/sfinder.jar`
- Command output pane

Fumen decoding/encoding, screenshot import, playable mode, output preview, and
the full advanced settings set are still expected follow-up work.

## Build On Linux

Install Qt 6 development packages and CMake, then run:

```sh
cd platforms/linux-qt
cmake -S . -B build
cmake --build build
./build/solution-finder-enhanced-qt
```

On Debian/Ubuntu-style systems, the packages are usually similar to:

```sh
sudo apt install cmake g++ qt6-base-dev default-jre
```

The app discovers the repository root by looking for `shared/openers.json`, so
run it from inside a clone of `solution-finder-enhanced` while this port is
early.
