#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
APP_PATH="$ROOT_DIR/dist/Solution Finder Enhanced.app"
DMG_PATH="$ROOT_DIR/dist/Solution Finder Enhanced.dmg"
STAGING_DIR=$(mktemp -d "${TMPDIR:-/tmp}/solution-finder-enhanced-dmg.XXXXXX")
STAGED_APP="$STAGING_DIR/Solution Finder Enhanced.app"

cleanup() {
    rm -rf "$STAGING_DIR"
}
trap cleanup EXIT INT TERM

"$SCRIPT_DIR/build_app.sh"

ditto --noextattr --noqtn "$APP_PATH" "$STAGED_APP"
xattr -cr "$STAGED_APP"
codesign --force --deep --sign - "$STAGED_APP"
codesign --verify --deep --strict --verbose=2 "$STAGED_APP"
ln -s /Applications "$STAGING_DIR/Applications"

hdiutil create \
    -volname "Solution Finder Enhanced" \
    -srcfolder "$STAGING_DIR" \
    -format UDZO \
    -ov \
    "$DMG_PATH"

echo "Built $DMG_PATH"
