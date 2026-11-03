#!/bin/bash
# Builds 20-20-20 for macOS with nothing but the Xcode Command Line Tools
# (xcode-select --install). Mirrors build.cmd on Windows: no project file, no dependencies.
#
#   ./build.sh            universal (arm64 + x86_64) .app and .dmg in mac/dist
#   ./build.sh --native   current architecture only (faster, for local testing)
#   ./build.sh --run      build native, then launch it
set -euo pipefail
cd "$(dirname "$0")"

APP_NAME="20-20-20"
EXE="TwentyTwentyTwenty"
MIN_OS="11.0"
NATIVE=0; RUN=0
for a in "$@"; do
  case "$a" in
    --native) NATIVE=1 ;;
    --run) NATIVE=1; RUN=1 ;;
    *) echo "unknown option: $a"; exit 1 ;;
  esac
done

if ! command -v swiftc >/dev/null 2>&1; then
  echo "swiftc not found. Install the Xcode Command Line Tools: xcode-select --install"; exit 1
fi

rm -rf build dist
mkdir -p build dist

SHARED=(Sources/App.swift Sources/Settings.swift Sources/Skin.swift Sources/Pixel.swift)
APP_SRC=(Sources/*.swift)
FLAGS=(-O -swift-version 5 -framework AppKit -framework ServiceManagement)

echo "[1/4] Generating icon..."
swiftc -O -parse-as-library -framework AppKit -framework ServiceManagement \
  -o build/MakeIcon tools/MakeIcon.swift "${SHARED[@]}"
build/MakeIcon build/app.iconset
iconutil -c icns build/app.iconset -o build/app.icns

echo "[2/4] Compiling $EXE..."
if [ "$NATIVE" = "1" ]; then
  swiftc "${FLAGS[@]}" -o "build/$EXE" "${APP_SRC[@]}"
else
  swiftc "${FLAGS[@]}" -target "arm64-apple-macos$MIN_OS"  -o "build/$EXE-arm64"  "${APP_SRC[@]}"
  swiftc "${FLAGS[@]}" -target "x86_64-apple-macos$MIN_OS" -o "build/$EXE-x86_64" "${APP_SRC[@]}"
  lipo -create -output "build/$EXE" "build/$EXE-arm64" "build/$EXE-x86_64"
fi

echo "[3/4] Bundling $APP_NAME.app..."
APP="dist/$APP_NAME.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "build/$EXE" "$APP/Contents/MacOS/$EXE"
cp build/app.icns "$APP/Contents/Resources/app.icns"
cp Resources/Info.plist "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
# Ad-hoc signature: enough to run locally and for SMAppService (login item) to work.
# Replace "-" with a Developer ID identity (and notarize) for distribution outside your own machines.
codesign --force --deep --sign "${CODESIGN_IDENTITY:--}" "$APP"

echo "[4/4] Disk image..."
STAGE=build/dmg
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -quiet -volname "$APP_NAME" -srcfolder "$STAGE" -ov -format UDZO "dist/$EXE-mac.dmg"

echo
echo "App:  $APP"
echo "DMG:  dist/$EXE-mac.dmg  ($(du -h "dist/$EXE-mac.dmg" | cut -f1))"

if [ "$RUN" = "1" ]; then
  pkill -x "$EXE" 2>/dev/null || true
  open "$APP"
fi
