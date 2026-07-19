# Solution Finder Enhanced macOS App

This folder contains the SwiftUI macOS frontend.

## Build And Run

Copy and paste from the repository root:

```sh
sh macos-native-gui/build_app.sh
open "dist/Solution Finder Enhanced.app"
```

The build script always rebuilds the release binary, then creates a self-contained
`.app` bundle in `dist/`.

Create a distributable test DMG:

```sh
sh macos-native-gui/build_dmg.sh
open "dist/Solution Finder Enhanced.dmg"
```

The local build is ad-hoc signed. Public distribution without Gatekeeper
warnings requires a Developer ID certificate and Apple notarization.

## Requirements

- macOS 13 or newer
- Xcode Command Line Tools
- Java runtime

Install the Swift build tools if needed:

```sh
xcode-select --install
```
