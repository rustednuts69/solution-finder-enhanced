# Solution Finder Enhanced macOS Builds

The packaged macOS release uses the shared Qt interface by default. The
original SwiftUI frontend remains available as an alternate build.

## Default Qt Build

Copy and paste from the repository root:

```sh
sh macos-native-gui/build_app.sh
open "dist/Solution Finder Enhanced.app"
```

The build script compiles the shared Qt frontend, runs the portable game-core
tests, deploys the required Qt frameworks, and creates a self-contained `.app`
bundle in `dist/`.

Create a distributable test DMG:

```sh
sh macos-native-gui/build_dmg.sh
open "dist/Solution Finder Enhanced.dmg"
```

The local build is ad-hoc signed. Public distribution without Gatekeeper
warnings requires a Developer ID certificate and Apple notarization.

## Alternate Swift Build

The previous Swift frontend is preserved and can still be built explicitly:

```sh
sh macos-native-gui/build_swift_app.sh
open "dist/Solution Finder Enhanced.app"
```

Both scripts write to the same app path, so the most recently built frontend is
the one in `dist/`.

## Qt Requirements

- macOS 13 or newer
- Qt 6
- CMake
- Xcode Command Line Tools
- Java runtime

Homebrew can install Qt and CMake:

```sh
brew install qt cmake
```

The alternate Swift build requires the Swift tools included with Xcode Command
Line Tools.
