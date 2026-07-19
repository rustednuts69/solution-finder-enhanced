#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${PROJECT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${PROJECT_DIR}/AppDir"
TOOLS_DIR="${PROJECT_DIR}/.appimage-tools"
APP_ID="solution-finder-enhanced"
APP_NAME="Solution Finder Enhanced"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "AppImage builds must run on Linux." >&2
  exit 1
fi

ARCH="$(uname -m)"
case "${ARCH}" in
  x86_64|aarch64) ;;
  *)
    echo "Unsupported AppImage architecture: ${ARCH}" >&2
    exit 1
    ;;
esac

download_tool() {
  local url="$1"
  local output="$2"
  mkdir -p "${TOOLS_DIR}"
  if [[ -x "${output}" ]]; then
    return
  fi
  if command -v curl >/dev/null 2>&1; then
    curl -L "${url}" -o "${output}"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "${output}" "${url}"
  else
    echo "Install curl or wget so this script can download AppImage packaging tools." >&2
    exit 1
  fi
  chmod +x "${output}"
}

LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-${ARCH}.AppImage"
QT_PLUGIN="${TOOLS_DIR}/linuxdeploy-plugin-qt-${ARCH}.AppImage"
download_tool "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage" "${LINUXDEPLOY}"
download_tool "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage" "${QT_PLUGIN}"
ln -sf "$(basename "${QT_PLUGIN}")" "${TOOLS_DIR}/linuxdeploy-plugin-qt"
export PATH="${TOOLS_DIR}:${PATH}"

rm -rf "${APPDIR}"
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" --config Release
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

mkdir -p "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
cp "${SCRIPT_DIR}/solution-finder-enhanced.png" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png"
cat > "${APPDIR}/usr/share/applications/${APP_ID}.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Exec=solution-finder-enhanced-qt
Icon=${APP_ID}
Categories=Game;Utility;
Terminal=false
DESKTOP

export OUTPUT="${PROJECT_DIR}/${APP_NAME// /-}-${ARCH}.AppImage"
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS:-platforms/libqxcb.so;imageformats}"
"${LINUXDEPLOY}" \
  --appdir "${APPDIR}" \
  --desktop-file "${APPDIR}/usr/share/applications/${APP_ID}.desktop" \
  --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png" \
  --plugin qt \
  --output appimage

echo "Built ${OUTPUT}"
