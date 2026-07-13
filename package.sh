#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PROFILE="${1:-licensed}"
PACKAGE_NAME="StringLife"
IDENTIFIER_BASE="com.mothproduction.stringlife"
VERSION="1.6.2"

case "$PROFILE" in
  licensed|local-dev)
    ;;
  *)
    echo "Usage: $0 [licensed|local-dev]" >&2
    exit 1
    ;;
esac

./build.sh "$PROFILE"

BUILD_DIR="build/$PROFILE"
PKG_DIR="$BUILD_DIR/pkg"
STAGING_DIR="$PKG_DIR/staging"
SCRIPTS_DIR="$PKG_DIR/scripts"
OUTPUT_PKG="$BUILD_DIR/${PACKAGE_NAME}-${PROFILE}.pkg"

APP_SOURCE="$BUILD_DIR/Standalone/StringLife.app"
VST3_SOURCE="$BUILD_DIR/VST3/StringLife.vst3"
AU_SOURCE="$BUILD_DIR/AU/StringLife.component"

for path in "$APP_SOURCE" "$VST3_SOURCE" "$AU_SOURCE"; do
  if [[ ! -e "$path" ]]; then
    echo "Missing build artefact: $path" >&2
    exit 1
  fi
done

rm -rf "$PKG_DIR"
mkdir -p \
  "$STAGING_DIR/Applications/Moth Production" \
  "$STAGING_DIR/Library/Audio/Plug-Ins/VST3" \
  "$STAGING_DIR/Library/Audio/Plug-Ins/Components" \
  "$SCRIPTS_DIR"

cp -R "$APP_SOURCE" "$STAGING_DIR/Applications/Moth Production/"
cp -R "$VST3_SOURCE" "$STAGING_DIR/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU_SOURCE" "$STAGING_DIR/Library/Audio/Plug-Ins/Components/"

cat > "$SCRIPTS_DIR/postinstall" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

# Refresh Launch Services so the standalone app appears immediately.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
  -f "/Applications/Moth Production/StringLife.app" >/dev/null 2>&1 || true

exit 0
EOF

chmod +x "$SCRIPTS_DIR/postinstall"

pkgbuild \
  --root "$STAGING_DIR" \
  --identifier "$IDENTIFIER_BASE.$PROFILE.installer" \
  --version "$VERSION" \
  --install-location "/" \
  --scripts "$SCRIPTS_DIR" \
  "$OUTPUT_PKG"

echo
echo "==> Installer package created:"
echo "    $OUTPUT_PKG"
echo
echo "    Installs to:"
echo "    /Applications/Moth Production/StringLife.app"
echo "    /Library/Audio/Plug-Ins/VST3/StringLife.vst3"
echo "    /Library/Audio/Plug-Ins/Components/StringLife.component"
