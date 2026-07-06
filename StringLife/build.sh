#!/usr/bin/env bash
# Builds StringLife (VST3 + AU + Standalone) and reports where the results
# ended up. COPY_PLUGIN_AFTER_BUILD in CMakeLists.txt already installs the
# VST3/AU into the system plugin folders (~/Library/Audio/Plug-Ins/...), so
# a DAW rescan is all that's needed afterwards - no manual copying required.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="build"
CONFIG="Release"
PROFILE="${1:-licensed}"

case "$PROFILE" in
  licensed)
    LICENSING_FLAG=ON
    BUILD_DIR="build/licensed"
    ;;
  local-dev)
    LICENSING_FLAG=OFF
    BUILD_DIR="build/local-dev"
    ;;
  *)
    echo "Usage: $0 [licensed|local-dev]" >&2
    exit 1
    ;;
esac

echo "==> Profile: $PROFILE"
echo "==> Configuring ($BUILD_DIR/)..."
cmake_args=(
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$CONFIG"
  -DSTRINGLIFE_ENABLE_LICENSING="$LICENSING_FLAG"
)

if [[ -n "${LICENSE_SERVER_BASE_URL:-}" ]]; then
  cmake_args+=(-DSTRINGLIFE_LICENSE_SERVER_BASE_URL="$LICENSE_SERVER_BASE_URL")
fi

if [[ -n "${LICENSE_PRODUCT_ID:-}" ]]; then
  cmake_args+=(-DSTRINGLIFE_LICENSE_PRODUCT_ID="$LICENSE_PRODUCT_ID")
fi

if [[ -n "${LICENSE_PUBLIC_KEY:-}" ]]; then
  cmake_args+=(-DSTRINGLIFE_LICENSE_PUBLIC_KEY="$LICENSE_PUBLIC_KEY")
fi

cmake "${cmake_args[@]}" > /dev/null

echo "==> Building VST3 + AU + Standalone ($CONFIG)..."
cmake --build "$BUILD_DIR" --config "$CONFIG" \
    --target StringLife_VST3 StringLife_AU StringLife_Standalone \
    -j "$(sysctl -n hw.ncpu)"

ARTEFACTS="$BUILD_DIR/StringLife_artefacts/$CONFIG"
[ -d "$ARTEFACTS" ] || ARTEFACTS="$BUILD_DIR/StringLife_artefacts"

FLAT_VST3_DIR="$BUILD_DIR/VST3"
FLAT_AU_DIR="$BUILD_DIR/AU"
FLAT_STANDALONE_DIR="$BUILD_DIR/Standalone"

rm -rf "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"
mkdir -p "$FLAT_VST3_DIR" "$FLAT_AU_DIR" "$FLAT_STANDALONE_DIR"

cp -R "$ARTEFACTS/VST3/StringLife.vst3" "$FLAT_VST3_DIR/"
cp -R "$ARTEFACTS/AU/StringLife.component" "$FLAT_AU_DIR/"
cp -R "$ARTEFACTS/Standalone/StringLife.app" "$FLAT_STANDALONE_DIR/"

echo
echo "==> Build complete."
echo "    Profile:    $PROFILE"
echo "    VST3:       $FLAT_VST3_DIR/StringLife.vst3"
echo "    AU:         $FLAT_AU_DIR/StringLife.component"
echo "    Standalone: $FLAT_STANDALONE_DIR/StringLife.app"
echo
echo "    Also installed system-wide (COPY_PLUGIN_AFTER_BUILD):"
echo "    ~/Library/Audio/Plug-Ins/VST3/StringLife.vst3"
echo "    ~/Library/Audio/Plug-Ins/Components/StringLife.component"
