ui_print "*************************************************"
ui_print "       Universal KeyMapper (Any Device)          "
ui_print "          Auto Key Detection & WebUI             "
ui_print "*************************************************"

# Detect Device Info
MANUFACTURER=$(getprop ro.product.manufacturer)
MODEL=$(getprop ro.product.model)
DEVICE=$(getprop ro.product.device)
ARCH=$(getprop ro.product.cpu.abi)

ui_print "- Device: $MANUFACTURER $MODEL ($DEVICE)"
ui_print "- Architecture: $ARCH"

if [ "$ARCH" != "arm64-v8a" ] && [ "$ARCH" != "aarch64" ]; then
    ui_print "! Unsupported architecture: $ARCH (Requires aarch64)"
    abort "! Installation aborted"
fi

# Set executable permissions on binary first so it can run detection
chmod 755 "$MODPATH/system/bin/keyactiond"
chmod 755 "$MODPATH/system/bin/keymapper"

ui_print " "
ui_print "- Scanning hardware input devices & keys on this device..."
DETECTED_OUT="$($MODPATH/system/bin/keyactiond -d 2>/dev/null)"
if [ -n "$DETECTED_OUT" ]; then
    echo "$DETECTED_OUT" | while IFS= read -r line; do
        ui_print "  $line"
    done
else
    ui_print "  ! Direct scan completed."
fi

# Config Generation / Preservation
CONFIG_TARGET="/data/adb/modules/essential_keymapper/keymapper.conf"
if [ -f "$CONFIG_TARGET" ]; then
    ui_print "- Preserving existing user key configuration..."
    cp -f "$CONFIG_TARGET" "$MODPATH/keymapper.conf"
else
    ui_print "- Auto-generating device-specific configuration profile..."
    "$MODPATH/system/bin/keyactiond" --generate-config "$MODPATH/keymapper.conf" 2>/dev/null || true
    if [ ! -f "$MODPATH/keymapper.conf" ] || [ ! -s "$MODPATH/keymapper.conf" ]; then
        ui_print "  - Using fallback universal profile..."
    fi
fi

# Setup Permissions
ui_print "- Setting binary, WebUI, and script permissions..."
set_perm "$MODPATH/system/bin/keyactiond" 0 0 0755
set_perm "$MODPATH/system/bin/keymapper" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755
set_perm "$MODPATH/keymapper.conf" 0 0 0644

if [ -d "$MODPATH/webroot" ]; then
    set_perm_recursive "$MODPATH/webroot" 0 0 0755 0644
fi

if [ -d "$MODPATH/system/usr/keylayout" ]; then
    set_perm_recursive "$MODPATH/system/usr/keylayout" 0 0 0755 0644
fi

if [ -d "$MODPATH/vendor/usr/keylayout" ]; then
    set_perm_recursive "$MODPATH/vendor/usr/keylayout" 0 0 0755 0644
fi

ui_print " "
ui_print "*************************************************"
ui_print " Installation successful!"
ui_print " "
ui_print " WebUI Manager:"
ui_print " - Open KernelSU / APatch / MMRL and tap 'WebUI'"
ui_print " - Includes real-time log viewer, key remapping,"
ui_print "   and live hardware button tester!"
ui_print " "
ui_print " Terminal CLI Commands:"
ui_print "   su -c keymapper status"
ui_print "   su -c keymapper detect"
ui_print "   su -c keymapper test"
ui_print "   su -c keymapper log"
ui_print "*************************************************"
