#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PROFILE="${1:-licensed}"
PACKAGE_NAME="StringLife"
IDENTIFIER_BASE="com.mothproduction.stringlife"

CODESIGN_IDENTITY="${CODESIGN_IDENTITY:-Developer ID Application: Márton Ferenczi (Y8272MS92K)}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-Developer ID Installer: Márton Ferenczi (Y8272MS92K)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-moth-notary}"

case "$PROFILE" in
  licensed|dev)
    ;;
  *)
    echo "Usage: $0 [licensed|dev]" >&2
    exit 1
    ;;
esac

./build.sh "$PROFILE"

BUILD_DIR="build/Release/$PROFILE"
VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD_DIR/CMakeCache.txt")"
if [[ ! "$VERSION" =~ ^[0-9]+(\.[0-9]+){0,3}$ ]]; then
  echo "Could not read the project version from $BUILD_DIR/CMakeCache.txt" >&2
  exit 1
fi

PKG_DIR="$BUILD_DIR/pkg"
COMPONENTS_DIR="$PKG_DIR/components"

if [[ "$PROFILE" == "licensed" ]]; then
  OUTPUT_PKG="$BUILD_DIR/${PACKAGE_NAME}-${VERSION}.pkg"
else
  OUTPUT_PKG="$BUILD_DIR/${PACKAGE_NAME}-${VERSION}-${PROFILE}.pkg"
fi

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
mkdir -p "$COMPONENTS_DIR"

ENTITLEMENTS="$PKG_DIR/entitlements.plist"
cat > "$ENTITLEMENTS" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.device.audio-input</key>
    <true/>
</dict>
</plist>
PLIST

# --- Standalone component -------------------------------------------------
STANDALONE_STAGING="$PKG_DIR/staging-standalone"
STANDALONE_SCRIPTS="$PKG_DIR/scripts-standalone"
mkdir -p "$STANDALONE_STAGING/Applications/Moth Production" "$STANDALONE_SCRIPTS"
cp -R "$APP_SOURCE" "$STANDALONE_STAGING/Applications/Moth Production/"

codesign --force --deep --options runtime --timestamp \
  --entitlements "$ENTITLEMENTS" \
  --sign "$CODESIGN_IDENTITY" \
  "$STANDALONE_STAGING/Applications/Moth Production/StringLife.app"

cat > "$STANDALONE_SCRIPTS/postinstall" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

# Refresh Launch Services so the standalone app appears immediately.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
  -f "/Applications/Moth Production/StringLife.app" >/dev/null 2>&1 || true

exit 0
EOF
chmod +x "$STANDALONE_SCRIPTS/postinstall"

pkgbuild \
  --root "$STANDALONE_STAGING" \
  --identifier "$IDENTIFIER_BASE.$PROFILE.standalone" \
  --version "$VERSION" \
  --install-location "/" \
  --scripts "$STANDALONE_SCRIPTS" \
  "$COMPONENTS_DIR/standalone.pkg"

# --- VST3 component ---------------------------------------------------------
VST3_STAGING="$PKG_DIR/staging-vst3"
mkdir -p "$VST3_STAGING/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3_SOURCE" "$VST3_STAGING/Library/Audio/Plug-Ins/VST3/"

codesign --force --options runtime --timestamp \
  --sign "$CODESIGN_IDENTITY" \
  "$VST3_STAGING/Library/Audio/Plug-Ins/VST3/StringLife.vst3"

pkgbuild \
  --root "$VST3_STAGING" \
  --identifier "$IDENTIFIER_BASE.$PROFILE.vst3" \
  --version "$VERSION" \
  --install-location "/" \
  "$COMPONENTS_DIR/vst3.pkg"

# --- AU component ------------------------------------------------------------
AU_STAGING="$PKG_DIR/staging-au"
mkdir -p "$AU_STAGING/Library/Audio/Plug-Ins/Components"
cp -R "$AU_SOURCE" "$AU_STAGING/Library/Audio/Plug-Ins/Components/"

codesign --force --options runtime --timestamp \
  --sign "$CODESIGN_IDENTITY" \
  "$AU_STAGING/Library/Audio/Plug-Ins/Components/StringLife.component"

pkgbuild \
  --root "$AU_STAGING" \
  --identifier "$IDENTIFIER_BASE.$PROFILE.au" \
  --version "$VERSION" \
  --install-location "/" \
  "$COMPONENTS_DIR/au.pkg"

# --- Distribution (lets the user choose which components to install) -------
DIST_XML="$PKG_DIR/distribution.xml"
cat > "$DIST_XML" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>StringLife</title>
    <organization>com.mothproduction</organization>
    <domains enable_localSystem="true"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <choices-outline>
        <line choice="standalone"/>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="standalone" title="Standalone application" description="Installs the standalone StringLife app into /Applications/Moth Production." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.standalone"/>
    </choice>
    <choice id="vst3" title="VST3 plug-in" description="Installs the StringLife VST3 plug-in into /Library/Audio/Plug-Ins/VST3." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU) plug-in" description="Installs the StringLife AU plug-in into /Library/Audio/Plug-Ins/Components." start_selected="true">
        <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.au"/>
    </choice>
    <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.standalone" version="$VERSION" onConclusion="none">standalone.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER_BASE.$PROFILE.au" version="$VERSION" onConclusion="none">au.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild \
  --distribution "$DIST_XML" \
  --package-path "$COMPONENTS_DIR" \
  --sign "$INSTALLER_IDENTITY" \
  "$OUTPUT_PKG"

echo
echo "==> Submitting for notarization (this can take a few minutes)..."
xcrun notarytool submit "$OUTPUT_PKG" --keychain-profile "$NOTARY_PROFILE" --wait

echo
echo "==> Stapling notarization ticket..."
xcrun stapler staple "$OUTPUT_PKG"

echo
echo "==> Installer package created:"
echo "    $OUTPUT_PKG"
echo
echo "    Signed, notarized and stapled. Lets the user pick any combination of:"
echo "    /Applications/Moth Production/StringLife.app"
echo "    /Library/Audio/Plug-Ins/VST3/StringLife.vst3"
echo "    /Library/Audio/Plug-Ins/Components/StringLife.component"
echo "    (all three are selected by default)"
