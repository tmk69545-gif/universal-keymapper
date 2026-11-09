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
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        // Redirect stdout/stderr to /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
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
    // Query screen state quickly via dumpsys power
    FILE *fp = popen("/system/bin/dumpsys power 2>/dev/null | grep -m1 'mWakefulness=Awake'", "r");
    if (!fp) return 1;
    char buf[128];
    char *res = fgets(buf, sizeof(buf), fp);
    pclose(fp);
    return (res != NULL);
}

void actions_toggle_flashlight(void) {
    run_forked_cmd(
        "cmd statusbar click-tile flashlight 2>/dev/null || "
        "if [ -f /sys/class/leds/flashlight/brightness ]; then "
        "  cur=$(cat /sys/class/leds/flashlight/brightness); "
        "  [ \"$cur\" -eq 0 ] && echo 255 > /sys/class/leds/flashlight/brightness || echo 0 > /sys/class/leds/flashlight/brightness; "
        "fi"
    );
}

void actions_screenshot(void) {
    run_forked_cmd("input keyevent 120"); // KEYCODE_SYSRQ
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
    run_forked_cmd("am start -a android.media.action.STILL_IMAGE_CAMERA 2>/dev/null || input keyevent 27");
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
    if (!action_str || !*action_str) return;
    if (strcasecmp(action_str, "default") == 0 || strcasecmp(action_str, "none") == 0) {
        return;
    }

    if (strcasecmp(action_str, "flashlight") == 0 || strcasecmp(action_str, "torch") == 0) {
        actions_toggle_flashlight();
    } else if (strcasecmp(action_str, "screenshot") == 0) {
        actions_screenshot();
    } else if (strcasecmp(action_str, "play_pause") == 0) {
        actions_play_pause();
    } else if (strcasecmp(action_str, "next_track") == 0) {
        actions_next_track();
    } else if (strcasecmp(action_str, "prev_track") == 0) {
        actions_prev_track();
    } else if (strcasecmp(action_str, "volume_up") == 0) {
        actions_volume_up();
    } else if (strcasecmp(action_str, "volume_down") == 0) {
        actions_volume_down();
    } else if (strcasecmp(action_str, "mute_toggle") == 0) {
        actions_mute_toggle();
    } else if (strcasecmp(action_str, "screen_off") == 0 || strcasecmp(action_str, "lock") == 0) {
        actions_screen_off();
    } else if (strcasecmp(action_str, "screen_on") == 0 || strcasecmp(action_str, "wake") == 0) {
        actions_screen_on();
    } else if (strcasecmp(action_str, "camera") == 0) {
        actions_camera();
    } else if (strcasecmp(action_str, "notifications") == 0) {
        actions_notifications();
    } else if (strcasecmp(action_str, "quick_settings") == 0) {
        actions_quick_settings();
    } else if (strcasecmp(action_str, "kill_app") == 0) {
        actions_kill_foreground_app();
    } else if (strncasecmp(action_str, "shell ", 6) == 0) {
        actions_run_shell(action_str + 6);
    }
}
