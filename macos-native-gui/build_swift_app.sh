#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
APP_DIR="$ROOT_DIR/dist/Solution Finder Enhanced.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"

cd "$SCRIPT_DIR"
export CLANG_MODULE_CACHE_PATH="$ROOT_DIR/build/swift-module-cache"
export SWIFTPM_CACHE_PATH="$ROOT_DIR/build/swiftpm-cache"
mkdir -p "$CLANG_MODULE_CACHE_PATH" "$SWIFTPM_CACHE_PATH"
swift build -c release

rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR" "$RESOURCES_DIR"
cp "$SCRIPT_DIR/.build/release/SolutionFinderEnhanced" "$MACOS_DIR/SolutionFinderEnhanced"
cp "$ROOT_DIR/fumen_en.html" "$RESOURCES_DIR/fumen_en.html"
cp "$ROOT_DIR/fumen.html" "$RESOURCES_DIR/fumen.html"
cp "$ROOT_DIR/shared/openers.json" "$RESOURCES_DIR/openers.json"
cp "$SCRIPT_DIR/Resources/AppIcon.icns" "$RESOURCES_DIR/AppIcon.icns"
cp -R "$ROOT_DIR/native-macos" "$RESOURCES_DIR/native-macos"
cp -R "$ROOT_DIR/solution-finder-1.43" "$RESOURCES_DIR/solution-finder-1.43"
find "$APP_DIR" -name .DS_Store -delete
rm -rf "$RESOURCES_DIR/native-macos/output" "$RESOURCES_DIR/solution-finder-1.43/output"
mkdir -p "$RESOURCES_DIR/native-macos/output" "$RESOURCES_DIR/solution-finder-1.43/output"
chmod +x "$RESOURCES_DIR/native-macos/bin/sfinder"

cat > "$CONTENTS_DIR/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>SolutionFinderEnhanced</string>
  <key>CFBundleIdentifier</key>
  <string>local.solution-finder-enhanced</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleIconFile</key>
  <string>AppIcon</string>
  <key>CFBundleName</key>
  <string>Solution Finder Enhanced</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleVersion</key>
  <string>1</string>
  <key>LSMinimumSystemVersion</key>
  <string>13.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
PLIST

echo "Built $APP_DIR"
