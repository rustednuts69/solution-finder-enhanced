#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$ROOT_DIR/build/macos-qt"
FINAL_APP_DIR="$ROOT_DIR/dist/Solution Finder Enhanced.app"
STAGING_DIR=$(mktemp -d "${TMPDIR:-/tmp}/solution-finder-enhanced-qt.XXXXXX")
APP_DIR="$STAGING_DIR/Solution Finder Enhanced.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"
SUPPORT_DIR="$CONTENTS_DIR/share/solution-finder-enhanced"
EXECUTABLE_NAME="solution-finder-enhanced-qt"

cleanup() {
    rm -rf "$STAGING_DIR"
}
trap cleanup EXIT INT TERM

QT_PATHS=${QT_PATHS:-$(command -v qtpaths6 || true)}
MACDEPLOYQT=${MACDEPLOYQT:-$(command -v macdeployqt || true)}
if [ -z "$QT_PATHS" ] || [ -z "$MACDEPLOYQT" ]; then
    echo "Qt 6 deployment tools were not found. Install Qt 6 and ensure qtpaths6 and macdeployqt are on PATH." >&2
    exit 1
fi
QT_PREFIX=$("$QT_PATHS" --query QT_INSTALL_PREFIX)
QT_PLUGIN_DIR=$("$QT_PATHS" --plugin-dir)

cmake \
    -S "$ROOT_DIR/platforms/linux-qt" \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD_DIR" --config Release
ctest --test-dir "$BUILD_DIR" --output-on-failure

mkdir -p "$MACOS_DIR" "$RESOURCES_DIR" "$SUPPORT_DIR"
cp "$BUILD_DIR/$EXECUTABLE_NAME" "$MACOS_DIR/$EXECUTABLE_NAME"
cp "$SCRIPT_DIR/Resources/AppIcon.icns" "$RESOURCES_DIR/AppIcon.icns"
mkdir -p \
    "$SUPPORT_DIR/shared" \
    "$SUPPORT_DIR/native-macos" \
    "$SUPPORT_DIR/solution-finder-1.43"
cp -R -X "$ROOT_DIR/shared/." "$SUPPORT_DIR/shared"
cp -R -X "$ROOT_DIR/native-macos/." "$SUPPORT_DIR/native-macos"
cp -R -X "$ROOT_DIR/solution-finder-1.43/." "$SUPPORT_DIR/solution-finder-1.43"
rm -rf \
    "$SUPPORT_DIR/native-macos/output" \
    "$SUPPORT_DIR/solution-finder-1.43/output"
find "$APP_DIR" -name .DS_Store -delete
chmod +x \
    "$MACOS_DIR/$EXECUTABLE_NAME" \
    "$SUPPORT_DIR/native-macos/bin/sfinder"

cat > "$CONTENTS_DIR/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>$EXECUTABLE_NAME</string>
  <key>CFBundleIdentifier</key>
  <string>local.solution-finder-enhanced</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleIconFile</key>
  <string>AppIcon</string>
  <key>CFBundleName</key>
  <string>Solution Finder Enhanced</string>
  <key>CFBundleDisplayName</key>
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

mkdir -p "$CONTENTS_DIR/PlugIns/platforms" "$CONTENTS_DIR/PlugIns/styles"
cp "$QT_PLUGIN_DIR/platforms/libqcocoa.dylib" "$CONTENTS_DIR/PlugIns/platforms/"
cp "$QT_PLUGIN_DIR/styles/libqmacstyle.dylib" "$CONTENTS_DIR/PlugIns/styles/"

xattr -cr "$APP_DIR"
"$MACDEPLOYQT" \
    "$APP_DIR" \
    -always-overwrite \
    -no-plugins \
    -no-codesign \
    "-executable=$CONTENTS_DIR/PlugIns/platforms/libqcocoa.dylib" \
    "-executable=$CONTENTS_DIR/PlugIns/styles/libqmacstyle.dylib"
xattr -cr "$APP_DIR"
codesign --force --deep --sign - "$APP_DIR"
codesign --verify --deep --strict "$APP_DIR"

rm -rf "$FINAL_APP_DIR"
ditto --noextattr --noqtn "$APP_DIR" "$FINAL_APP_DIR"
codesign --verify --deep --strict "$FINAL_APP_DIR"

echo "Built Qt app at $FINAL_APP_DIR"
