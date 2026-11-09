#!/system/bin/sh
# Universal KeyMapper CLI Control Utility
# Target: Any Android Device & AOSP/Voltage OS

MODDIR="/data/adb/modules/essential_keymapper"
DAEMON="/system/bin/keyactiond"
[ ! -x "$DAEMON" ] && DAEMON="$MODDIR/system/bin/keyactiond"

CONFIG="$MODDIR/keymapper.conf"
[ ! -f "$CONFIG" ] && CONFIG="/data/adb/keymapper/keymapper.conf"

LOGFILE="$MODDIR/keyactiond.log"
PIDFILE="$MODDIR/keyactiond.pid"

GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
BOLD="\033[1m"
RESET="\033[0m"

get_pid() {
    pidof keyactiond 2>/dev/null || pgrep -f keyactiond 2>/dev/null
}

show_status() {
    echo -e "${BOLD}=== Universal KeyMapper Status ===${RESET}"
    PID=$(get_pid)
    if [ -n "$PID" ]; then
        echo -e "Daemon Status: ${GREEN}RUNNING${RESET} (PID: $PID)"
        echo -e "Binary Path:   $DAEMON"
        echo -e "Config File:   $CONFIG"
        echo -e "WebUI URL:     ${CYAN}http://127.0.0.1:8321${RESET} (or open via KernelSU Next Manager)"
        if [ -f "$LOGFILE" ]; then
            echo -e "Log File:      $LOGFILE ($(wc -l < "$LOGFILE" 2>/dev/null || echo 0) lines)"
        fi
        echo ""
        echo -e "${BOLD}Auto-detected Hardware Keys on this Device:${RESET}"
        "$DAEMON" -d 2>/dev/null || echo "Unable to query keys directly."
    else
        echo -e "Daemon Status: ${RED}STOPPED${RESET}"
        echo -e "Run '${CYAN}keymapper start${RESET}' to launch daemon."
    fi
}

start_daemon() {
    PID=$(get_pid)
    if [ -n "$PID" ]; then
        echo -e "${YELLOW}Daemon is already running (PID: $PID).${RESET}"
        return 0
    fi

    if [ ! -x "$DAEMON" ]; then
        echo -e "${RED}Error: Binary $DAEMON not found or not executable!${RESET}"
        return 1
    fi

    echo -e "${CYAN}Starting Universal KeyMapper Daemon...${RESET}"
    nohup "$DAEMON" --config "$CONFIG" --webroot "$MODDIR/webroot" > "$LOGFILE" 2>&1 &
    sleep 0.5
    PID=$(get_pid)
    if [ -n "$PID" ]; then
        echo "$PID" > "$PIDFILE" 2>/dev/null
        echo -e "${GREEN}Started successfully (PID: $PID)${RESET}"
        echo -e "WebUI ready at: ${CYAN}http://127.0.0.1:8321${RESET}"
    else
        echo -e "${RED}Failed to start daemon. Check log:${RESET}"
        tail -n 15 "$LOGFILE" 2>/dev/null
    fi
}

stop_daemon() {
    PID=$(get_pid)
    if [ -z "$PID" ]; then
        echo -e "${YELLOW}Daemon is not running.${RESET}"
        return 0
    fi

    echo -e "${CYAN}Stopping Universal KeyMapper Daemon (PID: $PID)...${RESET}"
    kill -15 "$PID" 2>/dev/null
    sleep 0.5
    PID=$(get_pid)
    if [ -n "$PID" ]; then
        kill -9 "$PID" 2>/dev/null
    fi
    rm -f "$PIDFILE" 2>/dev/null
    echo -e "${GREEN}Daemon stopped.${RESET}"
}

reload_daemon() {
    PID=$(get_pid)
    if [ -z "$PID" ]; then
        echo -e "${YELLOW}Daemon is not running. Starting instead...${RESET}"
        start_daemon
        return
    fi

    echo -e "${CYAN}Sending reload signal to daemon (PID: $PID)...${RESET}"
    kill -USR1 "$PID" 2>/dev/null
    echo -e "${GREEN}Configuration reloaded without restarting daemon.${RESET}"
}

detect_keys() {
    if [ "$1" = "--json" ] || [ "$2" = "--json" ]; then
        "$DAEMON" --detect-keys-json
    else
        echo -e "${BOLD}Scanning all hardware input devices on this device...${RESET}"
        "$DAEMON" -d
    fi
}

test_mode() {
    PID=$(get_pid)
    WAS_RUNNING=0
    if [ -n "$PID" ]; then
        echo -e "${YELLOW}Temporarily stopping background daemon for interactive test...${RESET}"
        stop_daemon
        WAS_RUNNING=1
    fi

    echo -e "${CYAN}======================================================${RESET}"
    echo -e "${BOLD}Entering Interactive Hardware Key Test Mode${RESET}"
    echo -e "Press any hardware button on your phone."
    echo -e "Press ${YELLOW}Ctrl+C${RESET} to exit test mode."
    echo -e "${CYAN}======================================================${RESET}"

    "$DAEMON" --test --config "$CONFIG"

    if [ "$WAS_RUNNING" -eq 1 ]; then
        echo ""
        echo -e "${CYAN}Resuming background daemon...${RESET}"
        start_daemon
    fi
}

show_log() {
    if [ -f "$LOGFILE" ]; then
        tail -n 35 "$LOGFILE"
    else
        echo "No log file found at $LOGFILE"
    fi
}

show_help() {
    echo -e "${BOLD}Universal KeyMapper CLI Utility${RESET}"
    echo "Usage: keymapper <command>"
    echo ""
    echo "Commands:"
    echo "  status       Display daemon status, WebUI port, and detected keys"
    echo "  detect       Auto-detect all hardware keys on this device (table format)"
    echo "  detect --json Auto-detect all hardware keys in JSON format"
    echo "  start        Start background daemon and WebUI"
    echo "  stop         Stop background daemon"
    echo "  restart      Restart daemon"
    echo "  reload       Reload keymapper.conf on the fly"
    echo "  test         Interactive live test mode (press keys to test)"
    echo "  log          View recent daemon activity log"
    echo "  config       Show current keymapper.conf configuration"
    echo "  help         Display this help message"
}

case "$1" in
    status)
        show_status
        ;;
    detect)
        detect_keys "$@"
        ;;
    start)
        start_daemon
        ;;
    stop)
        stop_daemon
        ;;
    restart)
        stop_daemon
        sleep 0.5
        start_daemon
        ;;
    reload)
        reload_daemon
        ;;
    test)
        test_mode
        ;;
    log)
        show_log
        ;;
    config)
        cat "$CONFIG" 2>/dev/null || echo "Config file not found at $CONFIG"
        ;;
    help|--help|-h|"")
        show_help
        ;;
    *)
        echo "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
