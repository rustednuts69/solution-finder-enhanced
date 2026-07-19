# Solution Finder Enhanced Qt UI

This is the native Qt frontend for the Linux build of Solution Finder Enhanced.
Its interface and application logic live in `platforms/qt-common` and are shared
with the Windows build, while Linux keeps its own packaging and screenshot-tool
integration.

## Current Features

- Native Qt Widgets layout
- 10x20 editable board with click and drag painting
- Opener group and variation selector from `shared/openers.json`
- Native fumen page controls, mino placement controls, and fumen output code
- Play tab with guideline gameplay, configurable tuning, scoring, and PC Scout
- Embedded Output and read-only fumen Preview tabs
- Core sfinder command controls
- Process runner for the bundled `solution-finder-1.43/sfinder.jar`
- Command output pane
- Screenshot board import
- Screenshot opener detector
- Embedded HTML output with fumen-link preview handoff

Spin and REN commands and Play scouts are experimental and hidden by default.
Enable them from **Advanced > Enable Experimental Features**.

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

Screenshot import uses your desktop's region screenshot tool instead of drawing
its own capture overlay. `flameshot` is recommended because it works across more
desktop setups than `gnome-screenshot` or `scrot`. Install one of these if your
desktop does not already include one:

```sh
# Debian/Ubuntu examples
sudo apt install -y flameshot
# Wayland/wlroots users can use:
sudo apt install -y grim slurp
```

Then build and run:

```sh
git clone https://github.com/rustednuts69/solution-finder-enhanced.git
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

The development build discovers the repository root by looking for
`shared/openers.json`, so run it from inside the repository clone. AppImage
builds include the opener book and sfinder runtime.

## Build An AppImage

Run this on Linux after installing the same Qt build dependencies above. The
script downloads `linuxdeploy` and the Qt plugin into
`platforms/linux-qt/.appimage-tools` if they are not already present.

```sh
cd solution-finder-enhanced/platforms/linux-qt
./packaging/build-appimage.sh
```

The AppImage includes the Qt executable, `shared/openers.json`, and the bundled
`solution-finder-1.43` sfinder files under `usr/share/solution-finder-enhanced`.
