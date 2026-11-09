#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_NAME="Universal-KeyMapper-v2.0.0.zip"
OUTPUT_DIR="${1:-$REPO_DIR/dist}"

if [[ "$OUTPUT_DIR" != /* ]]; then
  OUTPUT_DIR="$REPO_DIR/$OUTPUT_DIR"
fi

mkdir -p "$OUTPUT_DIR"
TARGET_ZIP="$OUTPUT_DIR/$OUTPUT_NAME"

echo "=== Building Universal KeyMapper (Any Device & WebUI) ==="

# Step 1: Compile Native Daemon
echo "[*] Compiling native aarch64 daemon (keyactiond)..."
cd "$REPO_DIR/src"
make clean
make -j$(nproc)

mkdir -p "$REPO_DIR/system/bin"
cp -f "$REPO_DIR/src/keyactiond" "$REPO_DIR/system/bin/keyactiond"
cp -f "$REPO_DIR/keymapper.sh" "$REPO_DIR/system/bin/keymapper"

# Step 2: Package Module
echo "[*] Packaging module to: $TARGET_ZIP"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

cp "$REPO_DIR/module.prop" "$TMP_DIR/"
cp "$REPO_DIR/customize.sh" "$TMP_DIR/"
cp "$REPO_DIR/service.sh" "$TMP_DIR/"
cp "$REPO_DIR/uninstall.sh" "$TMP_DIR/"
cp "$REPO_DIR/keymapper.conf" "$TMP_DIR/"
cp -r "$REPO_DIR/META-INF" "$TMP_DIR/"
cp -r "$REPO_DIR/system" "$TMP_DIR/"
cp -r "$REPO_DIR/vendor" "$TMP_DIR/"
cp -r "$REPO_DIR/webroot" "$TMP_DIR/"

chmod 755 "$TMP_DIR/system/bin/"*
chmod 755 "$TMP_DIR/service.sh" "$TMP_DIR/customize.sh" "$TMP_DIR/uninstall.sh" "$TMP_DIR/META-INF/com/google/android/update-binary"
chmod 644 "$TMP_DIR/module.prop" "$TMP_DIR/keymapper.conf" "$TMP_DIR/META-INF/com/google/android/updater-script"
find "$TMP_DIR/webroot" -type f -exec chmod 644 {} +
find "$TMP_DIR/system/usr/keylayout" -type f -exec chmod 644 {} + 2>/dev/null || true
find "$TMP_DIR/vendor/usr/keylayout" -type f -exec chmod 644 {} + 2>/dev/null || true

cd "$TMP_DIR"
zip -r -q "$TARGET_ZIP" .

echo "[+] Built successfully: $TARGET_ZIP"
ls -lh "$TARGET_ZIP"

# Step 3: Optional push to connected device
if command -v adb >/dev/null 2>&1; then
    DEVICE_COUNT=$(adb devices | grep -v "List" | grep -c "device" || true)
    if [ "$DEVICE_COUNT" -gt 0 ]; then
        echo "[*] Connected ADB device detected. Pushing module to /sdcard/Download/..."
        adb push "$TARGET_ZIP" /sdcard/Download/ || true
        echo "[+] Flashable zip available on device: /sdcard/Download/$OUTPUT_NAME"
    fi
fi

echo "=== All Done! ==="
