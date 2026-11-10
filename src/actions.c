#include "actions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

static void run_forked_cmd(const char *cmd) {
    if (!cmd || !*cmd) return;
    printf("[EXEC] %s\n", cmd);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        // Redirect stdout/stderr to keyactiond.log so execution errors are captured
        int log_fd = open("/data/adb/modules/essential_keymapper/keyactiond.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        } else {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
        }
        execl("/system/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    // Parent returns immediately, SIGCHLD handled or reaped asynchronously
}

void actions_init(void) {
    // Ignore SIGCHLD to automatically reap zombie child processes
    signal(SIGCHLD, SIG_IGN);
}

int actions_is_screen_on(void) {
    // 1. Ultra-fast direct sysfs check (non-blocking, <0.1ms)
    FILE *fp = fopen("/sys/class/leds/lcd-backlight/brightness", "r");
    if (fp) {
        int val = 0;
        if (fscanf(fp, "%d", &val) == 1) {
            fclose(fp);
            return (val > 0);
        }
        fclose(fp);
    }

    // 2. Fallback to dumpsys power
    fp = popen("/system/bin/dumpsys power 2>/dev/null | grep -m1 'mWakefulness=Awake'", "r");
    if (!fp) return 1;
    char buf[128];
    char *res = fgets(buf, sizeof(buf), fp);
    pclose(fp);
    return (res != NULL);
}

void actions_toggle_flashlight(void) {
    run_forked_cmd(
        // 1. Android CameraManager via persistent app_process helper (universal across all Android 10-17 devices)
        "if [ -f /data/adb/modules/essential_keymapper/torch.jar ]; then "
        "  PIDFILE=/data/adb/modules/essential_keymapper/.torch.pid; "
        "  if [ -f \"$PIDFILE\" ]; then "
        "    PID=$(cat \"$PIDFILE\" 2>/dev/null); "
        "    if [ -n \"$PID\" ] && kill -0 \"$PID\" 2>/dev/null; then "
        "      kill -9 \"$PID\" 2>/dev/null; "
        "      rm -f \"$PIDFILE\"; "
        "      exit 0; "
        "    fi; "
        "    rm -f \"$PIDFILE\"; "
        "  fi; "
        "  nohup app_process -Djava.class.path=/data/adb/modules/essential_keymapper/torch.jar /system/bin Torch 1 >/dev/null 2>&1 & "
        "  echo $! > \"$PIDFILE\"; "
        // 2. MediaTek MTK Torch sysfs
        "elif [ -f /sys/devices/virtual/flashlight_core/flashlight/flashlight_torch ]; then "
        "  if grep -q '0 0 0 1' /sys/devices/virtual/flashlight_core/flashlight/flashlight_torch 2>/dev/null; then "
        "    echo '0 0 0 0' > /sys/devices/virtual/flashlight_core/flashlight/flashlight_torch; "
        "  else "
        "    echo '0 0 0 1' > /sys/devices/virtual/flashlight_core/flashlight/flashlight_torch; "
        "  fi; "
        // 3. Standard Linux sysfs leds
        "elif [ -f /sys/class/leds/flashlight/brightness ]; then "
        "  cur=$(cat /sys/class/leds/flashlight/brightness 2>/dev/null || echo 0); "
        "  [ \"$cur\" -eq 0 ] && echo 255 > /sys/class/leds/flashlight/brightness || echo 0 > /sys/class/leds/flashlight/brightness; "
        "elif [ -f /sys/class/leds/torch-light/brightness ]; then "
        "  cur=$(cat /sys/class/leds/torch-light/brightness 2>/dev/null || echo 0); "
        "  [ \"$cur\" -eq 0 ] && echo 255 > /sys/class/leds/torch-light/brightness || echo 0 > /sys/class/leds/torch-light/brightness; "
        "else "
        "  cmd statusbar click-tile flashlight 2>/dev/null; "
        "fi"
    );
}

void actions_screenshot(void) {
    run_forked_cmd(
        // Cascading screenshot methods — first success wins
        // 1. cmd statusbar screenshot (Android 12+ / Nothing phones — most reliable)
        "if cmd statusbar screenshot 2>/dev/null; then exit 0; fi; "
        // 2. svc screenshot (Android 14+)
        "if svc screenshot 2>/dev/null; then exit 0; fi; "
        // 3. KEYCODE_SCREENSHOT (keyevent 220)
        "if input keyevent 220 2>/dev/null; then exit 0; fi; "
        // 4. KEYCODE_SYSRQ (keyevent 120) — legacy method
        "if input keyevent 120 2>/dev/null; then exit 0; fi; "
        // 5. Final fallback: screencap to PNG file + media scan so it shows in gallery
        "SPATH=/sdcard/Pictures/Screenshots; "
        "mkdir -p \"$SPATH\"; "
        "FNAME=\"$SPATH/Screenshot_$(date +%Y%m%d_%H%M%S).png\"; "
        "screencap -p \"$FNAME\" 2>/dev/null && "
        "am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE "
        "-d \"file://$FNAME\" >/dev/null 2>&1"
    );
}

void actions_play_pause(void) {
    run_forked_cmd("cmd media_session dispatch play-pause || input keyevent 85");
}

void actions_next_track(void) {
    run_forked_cmd("cmd media_session dispatch next || input keyevent 87");
}

void actions_prev_track(void) {
    run_forked_cmd("cmd media_session dispatch previous || input keyevent 88");
}

void actions_volume_up(void) {
    run_forked_cmd("cmd audio adj-volume STREAM_MUSIC RAISE || input keyevent 24");
}

void actions_volume_down(void) {
    run_forked_cmd("cmd audio adj-volume STREAM_MUSIC LOWER || input keyevent 25");
}

void actions_mute_toggle(void) {
    run_forked_cmd(
        "cur=$(settings get global mode_ringer 2>/dev/null || echo 2); "
        "if [ \"$cur\" -eq 2 ]; then "
        "  cmd audio set-ringer-mode VIBRATE 2>/dev/null || settings put global mode_ringer 1; "
        "elif [ \"$cur\" -eq 1 ]; then "
        "  cmd audio set-ringer-mode SILENT 2>/dev/null || settings put global mode_ringer 0; "
        "else "
        "  cmd audio set-ringer-mode NORMAL 2>/dev/null || settings put global mode_ringer 2; "
        "fi"
    );
}

void actions_screen_off(void) {
    run_forked_cmd("input keyevent 26"); // KEYCODE_POWER
}

void actions_screen_on(void) {
    run_forked_cmd("input keyevent 224"); // KEYCODE_WAKEUP
}

void actions_camera(void) {
    run_forked_cmd(
        "am start -a android.media.action.STILL_IMAGE_CAMERA_SECURE 2>/dev/null || "
        "am start -a android.media.action.STILL_IMAGE_CAMERA 2>/dev/null || "
        "input keyevent 27"
    );
}

void actions_notifications(void) {
    run_forked_cmd("cmd statusbar expand-notifications 2>/dev/null");
}

void actions_quick_settings(void) {
    run_forked_cmd("cmd statusbar expand-settings 2>/dev/null");
}

void actions_kill_foreground_app(void) {
    run_forked_cmd(
        "pkg=$(dumpsys window 2>/dev/null | grep -E 'mCurrentFocus|mFocusedApp' | head -n1 | sed -e 's/.* //;s/\\/.*//'); "
        "if [ -n \"$pkg\" ] && [ \"$pkg\" != \"com.android.launcher3\" ] && [ \"$pkg\" != \"com.android.systemui\" ]; then "
        "  am force-stop \"$pkg\"; "
        "fi"
    );
}

void actions_run_shell(const char *cmd) {
    if (cmd && *cmd) {
        run_forked_cmd(cmd);
    }
}

void actions_execute(const char *action_str) {
    if (!action_str) return;
    while (*action_str == ' ' || *action_str == '\t' || *action_str == '\r' || *action_str == '\n') {
        action_str++;
    }
    if (!*action_str) return;

    char act[MAX_ACTION_LEN];
    strncpy(act, action_str, sizeof(act) - 1);
    act[sizeof(act) - 1] = '\0';
    char *end = act + strlen(act) - 1;
    while (end >= act && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    if (strcasecmp(act, "default") == 0 || strcasecmp(act, "none") == 0) {
        return;
    }

    if (strcasecmp(act, "flashlight") == 0 || strcasecmp(act, "torch") == 0) {
        actions_toggle_flashlight();
    } else if (strcasecmp(act, "screenshot") == 0) {
        actions_screenshot();
    } else if (strcasecmp(act, "play_pause") == 0) {
        actions_play_pause();
    } else if (strcasecmp(act, "next_track") == 0) {
        actions_next_track();
    } else if (strcasecmp(act, "prev_track") == 0) {
        actions_prev_track();
    } else if (strcasecmp(act, "volume_up") == 0) {
        actions_volume_up();
    } else if (strcasecmp(act, "volume_down") == 0) {
        actions_volume_down();
    } else if (strcasecmp(act, "mute_toggle") == 0) {
        actions_mute_toggle();
    } else if (strcasecmp(act, "screen_off") == 0 || strcasecmp(act, "lock") == 0) {
        actions_screen_off();
    } else if (strcasecmp(act, "screen_on") == 0 || strcasecmp(act, "wake") == 0) {
        actions_screen_on();
    } else if (strcasecmp(act, "camera") == 0) {
        actions_camera();
    } else if (strcasecmp(act, "notifications") == 0) {
        actions_notifications();
    } else if (strcasecmp(act, "quick_settings") == 0) {
        actions_quick_settings();
    } else if (strcasecmp(act, "kill_app") == 0) {
        actions_kill_foreground_app();
    } else if (strncasecmp(act, "shell ", 6) == 0) {
        actions_run_shell(act + 6);
    } else {
        printf("[WARN] Unknown action: '%s'\n", act);
        fflush(stdout);
    }
}
