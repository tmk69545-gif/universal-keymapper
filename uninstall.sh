#!/system/bin/sh
# Essential KeyMapper Uninstall Script
pidof keyactiond 2>/dev/null | xargs kill -9 2>/dev/null
rm -f /data/adb/modules/essential_keymapper/keyactiond.pid 2>/dev/null
rm -f /data/adb/modules/essential_keymapper/keyactiond.log 2>/dev/null
