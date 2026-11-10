#!/system/bin/sh
# Universal KeyMapper Background Service
MODDIR=${0%/*}

# Wait for Android boot completion
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
done

# Extra settling delay
sleep 1

DAEMON="$MODDIR/system/bin/keyactiond"
[ ! -x "$DAEMON" ] && DAEMON="/system/bin/keyactiond"

CONFIG="$MODDIR/keymapper.conf"
WEBROOT="$MODDIR/webroot"
LOGFILE="$MODDIR/keyactiond.log"
PIDFILE="$MODDIR/keyactiond.pid"

if [ -x "$DAEMON" ]; then
    pidof keyactiond 2>/dev/null | xargs kill -9 2>/dev/null

    nohup "$DAEMON" --config "$CONFIG" --webroot "$WEBROOT" > "$LOGFILE" 2>&1 &
    PID=$!
    echo "$PID" > "$PIDFILE" 2>/dev/null
fi
