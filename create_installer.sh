#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────────────────
# Steinbach Chanel Strip  –  macOS Installer Builder
# Erstellt ein .pkg auf dem Desktop, das VST3 + AU installiert.
# ────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build_release"
VERSION="1.0.1"
PLUGIN_NAME="Steinbach Chanel Strip"
IDENTIFIER_BASE="com.steinbach.chanelstrip"
DESKTOP="$HOME/Desktop"
PKG_OUT="$DESKTOP/SteinbachChanelStrip-${VERSION}.pkg"
WORK_DIR="/tmp/steinbach_installer_$$"

echo "▶  Release konfigurieren …"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SUPPRESS_DEVELOPER_WARNINGS=ON \
      2>&1 | tail -5

echo "▶  Release bauen …"
cmake --build "$BUILD_DIR" --parallel
echo "   ✓ Build fertig"

ARTEFACTS="$BUILD_DIR/SteinbachChanelStrip_artefacts/Release"
VST3_SRC="$ARTEFACTS/VST3/${PLUGIN_NAME}.vst3"
AU_SRC="$ARTEFACTS/AU/${PLUGIN_NAME}.component"

# ── Temporäre Ordnerstruktur ─────────────────────────────────────────────────
mkdir -p "$WORK_DIR/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "$WORK_DIR/au/Library/Audio/Plug-Ins/Components"
mkdir -p "$WORK_DIR/pkgs"

cp -r "$VST3_SRC"  "$WORK_DIR/vst3/Library/Audio/Plug-Ins/VST3/"
cp -r "$AU_SRC"    "$WORK_DIR/au/Library/Audio/Plug-Ins/Components/"

echo "▶  Komponenten-Pakete erstellen …"

pkgbuild \
  --root           "$WORK_DIR/vst3" \
  --install-location / \
  --identifier     "${IDENTIFIER_BASE}.vst3" \
  --version        "$VERSION" \
  "$WORK_DIR/pkgs/vst3.pkg"

pkgbuild \
  --root           "$WORK_DIR/au" \
  --install-location / \
  --identifier     "${IDENTIFIER_BASE}.au" \
  --version        "$VERSION" \
  "$WORK_DIR/pkgs/au.pkg"

# ── Distribution-XML ────────────────────────────────────────────────────────
cat > "$WORK_DIR/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${PLUGIN_NAME} ${VERSION}</title>
    <welcome    file="welcome.html"    mime-type="text/html"/>
    <background file="background.png" mime-type="image/png"
                scaling="proportional" alignment="bottomleft"/>
    <options customize="never" require-scripts="false" rootVolumeOnly="true"/>

    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>

    <choice id="vst3" title="VST3 Plugin"
            description="Installiert das VST3-Plugin nach /Library/Audio/Plug-Ins/VST3/">
        <pkg-ref id="${IDENTIFIER_BASE}.vst3"/>
    </choice>

    <choice id="au" title="Audio Unit (AU)"
            description="Installiert das AU-Plugin nach /Library/Audio/Plug-Ins/Components/">
        <pkg-ref id="${IDENTIFIER_BASE}.au"/>
    </choice>

    <pkg-ref id="${IDENTIFIER_BASE}.vst3" version="${VERSION}">vst3.pkg</pkg-ref>
    <pkg-ref id="${IDENTIFIER_BASE}.au"   version="${VERSION}">au.pkg</pkg-ref>
</installer-gui-script>
XML

# Einfaches HTML-Willkommen (productbuild ignoriert fehlende welcome-Datei nicht → Fallback)
cat > "$WORK_DIR/welcome.html" <<HTML
<!DOCTYPE html>
<html><body style="font-family:system-ui;padding:20px">
<h2>${PLUGIN_NAME} ${VERSION}</h2>
<p>Installiert VST3 und Audio Unit in die System-Plug-In-Ordner.<br>
Administratorrechte werden benötigt.</p>
</body></html>
HTML

echo "▶  Finales Installer-Paket zusammenstellen …"
productbuild \
  --distribution "$WORK_DIR/distribution.xml" \
  --package-path "$WORK_DIR/pkgs" \
  --version      "$VERSION" \
  "$PKG_OUT"

# ── Aufräumen ────────────────────────────────────────────────────────────────
rm -rf "$WORK_DIR"

echo ""
echo "✅  Installer fertig:"
echo "    $PKG_OUT"
echo ""
echo "   Doppelklick → installiert VST3 + AU systemweit."
echo "   (Gatekeeper: Rechtsklick → Öffnen, falls Warnung erscheint)"
