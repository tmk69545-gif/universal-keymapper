#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <linux/input.h>

#include "config.h"
#include "actions.h"
#include "device.h"
#include "http_server.h"
#include "events.h"

#define DEFAULT_CONFIG_PATH "/data/adb/modules/essential_keymapper/keymapper.conf"
#define DEFAULT_WEBROOT_PATH "/data/adb/modules/essential_keymapper/webroot"
#define DEFAULT_LOG_PATH "/data/adb/modules/essential_keymapper/keyactiond.log"
#define FALLBACK_CONFIG_PATH "/data/adb/keymapper/keymapper.conf"
#define LOCAL_CONFIG_PATH "./keymapper.conf"
#define LOCAL_WEBROOT_PATH "./webroot"
#define LOCAL_LOG_PATH "./keyactiond.log"

static const char *g_config_path = DEFAULT_CONFIG_PATH;
static const char *g_webroot_path = DEFAULT_WEBROOT_PATH;
static const char *g_log_path = DEFAULT_LOG_PATH;

volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_reload_requested = 0;
static int g_test_mode = 0;

typedef struct {
    int keycode;
    char name[64];
    key_config_t *cfg;
    int is_down;
    uint64_t press_time_ms;
    uint64_t release_time_ms;
    int click_count;
    int long_press_fired;
    int timer_fd;
} monitored_key_t;

static monitored_key_t g_monitored_keys[MAX_CONFIG_KEYS];
static int g_monitored_count = 0;

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void set_key_timer(monitored_key_t *mk, int ms) {
    if (mk->timer_fd < 0) return;
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    if (ms > 0) {
        its.it_value.tv_sec = ms / 1000;
        its.it_value.tv_nsec = (ms % 1000) * 1000000L;
    }
    timerfd_settime(mk->timer_fd, 0, &its, NULL);
}

static void disarm_key_timer(monitored_key_t *mk) {
    set_key_timer(mk, 0);
}

static monitored_key_t *find_key_by_code(int code) {
    for (int i = 0; i < g_monitored_count; i++) {
        if (g_monitored_keys[i].keycode == code) {
            return &g_monitored_keys[i];
        }
    }
    return NULL;
}

static monitored_key_t *find_key_by_timer(int tfd) {
    for (int i = 0; i < g_monitored_count; i++) {
        if (g_monitored_keys[i].timer_fd == tfd) {
            return &g_monitored_keys[i];
        }
    }
    return NULL;
}

static void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
    } else if (sig == SIGHUP || sig == SIGUSR1) {
        g_reload_requested = 1;
    }
}

static void trigger_gesture_action(monitored_key_t *mk, const char *gesture, const char *action_on, const char *action_off) {
    int screen_on = actions_is_screen_on();
    const char *action = screen_on ? action_on : action_off;

    if (!screen_on && (!action || !*action || strcasecmp(action, "default") == 0)) {
        action = action_on;
    }

    if (g_test_mode) {
        printf("\033[1;32m[ACTION TRIGGERED]\033[0m Key: %s (%d) | Gesture: %s | Screen: %s | Action: %s\n",
            mk->name, mk->keycode, gesture, screen_on ? "ON" : "OFF", (action && *action) ? action : "default");
        fflush(stdout);
    } else {
        if (g_config.log_level >= 1) {
            printf("[ACTION] Key: %s (%d) | %s | Screen: %s -> %s\n",
                mk->name, mk->keycode, gesture, screen_on ? "ON" : "OFF", (action && *action) ? action : "default");
            fflush(stdout);
        }
    }

    if (action && *action) {
        actions_execute(action);
    }
}

static int check_combos(monitored_key_t *current) {
    (void)current;
    monitored_key_t *volup = find_key_by_code(115);
    monitored_key_t *voldown = find_key_by_code(114);
    monitored_key_t *power = find_key_by_code(116);
    monitored_key_t *essential = find_key_by_code(250);

    // VolUp + VolDown
    if (volup && voldown && volup->is_down && voldown->is_down) {
        if (g_config.combo_volup_voldown[0] && strcasecmp(g_config.combo_volup_voldown, "none") != 0) {
            volup->long_press_fired = 1;
            voldown->long_press_fired = 1;
            disarm_key_timer(volup);
            disarm_key_timer(voldown);
            volup->click_count = 0;
            voldown->click_count = 0;
            if (g_test_mode) {
                printf("\033[1;35m[COMBO TRIGGERED]\033[0m VolUp + VolDown -> %s\n", g_config.combo_volup_voldown);
            }
            actions_execute(g_config.combo_volup_voldown);
            return 1;
        }
    }

    // Essential + VolDown
    if (essential && voldown && essential->is_down && voldown->is_down) {
        if (g_config.combo_essential_voldown[0] && strcasecmp(g_config.combo_essential_voldown, "none") != 0) {
            essential->long_press_fired = 1;
            voldown->long_press_fired = 1;
            disarm_key_timer(essential);
            disarm_key_timer(voldown);
            essential->click_count = 0;
            voldown->click_count = 0;
            if (g_test_mode) {
                printf("\033[1;35m[COMBO TRIGGERED]\033[0m Essential + VolDown -> %s\n", g_config.combo_essential_voldown);
            }
            actions_execute(g_config.combo_essential_voldown);
            return 1;
        }
    }

    // Essential + VolUp
    if (essential && volup && essential->is_down && volup->is_down) {
        if (g_config.combo_essential_volup[0] && strcasecmp(g_config.combo_essential_volup, "none") != 0) {
            essential->long_press_fired = 1;
            volup->long_press_fired = 1;
            disarm_key_timer(essential);
            disarm_key_timer(volup);
            essential->click_count = 0;
            volup->click_count = 0;
            if (g_test_mode) {
                printf("\033[1;35m[COMBO TRIGGERED]\033[0m Essential + VolUp -> %s\n", g_config.combo_essential_volup);
            }
            actions_execute(g_config.combo_essential_volup);
            return 1;
        }
    }

    // Power + VolUp
    if (power && volup && power->is_down && volup->is_down) {
        if (g_config.combo_power_volup[0] && strcasecmp(g_config.combo_power_volup, "none") != 0 && strcasecmp(g_config.combo_power_volup, "default") != 0) {
            power->long_press_fired = 1;
            volup->long_press_fired = 1;
            disarm_key_timer(power);
            disarm_key_timer(volup);
            power->click_count = 0;
            volup->click_count = 0;
            actions_execute(g_config.combo_power_volup);
            return 1;
        }
    }

    return 0;
}

static void handle_key_event(monitored_key_t *mk, int value) {
    uint64_t now = get_time_ms();
    uint64_t duration = (value == 0 && mk->press_time_ms > 0) ? (now - mk->press_time_ms) : 0;

    event_record(mk->keycode, mk->name, mk->cfg ? mk->cfg->label : mk->name, "hardware", value, duration, NULL);

    if (g_test_mode) {
        printf("\033[1;36m[KEY EVENT]\033[0m Code: %-3d (%s) | State: %s | Time: %llu ms\n",
            mk->keycode, mk->name,
            value == 1 ? "\033[32mDOWN\033[0m" : (value == 0 ? "\033[31mUP\033[0m" : "\033[33mREPEAT\033[0m"),
            (unsigned long long)now);
        fflush(stdout);
    } else if (g_config.log_level >= 1) {
        printf("[KEY EVENT] %s (%d) | State: %s | Duration: %llu ms\n",
            mk->name, mk->keycode, value == 1 ? "DOWN" : (value == 0 ? "UP" : "REPEAT"), (unsigned long long)duration);
        fflush(stdout);
    }

    if (value == 1) { // KEY_DOWN
        mk->is_down = 1;
        mk->press_time_ms = now;
        mk->long_press_fired = 0;

        if (check_combos(mk)) {
            return;
        }

        set_key_timer(mk, g_config.long_press_ms);

    } else if (value == 2) { // KEY_REPEAT
        if (mk->is_down && !mk->long_press_fired) {
            if ((now - mk->press_time_ms) >= (uint64_t)g_config.long_press_ms) {
                mk->long_press_fired = 1;
                disarm_key_timer(mk);
                mk->click_count = 0;
                trigger_gesture_action(mk, "Long Press", mk->cfg->long_press, mk->cfg->screen_off_long);
            }
        }
    } else if (value == 0) { // KEY_UP
        mk->is_down = 0;
        mk->release_time_ms = now;
        disarm_key_timer(mk);

        if (mk->long_press_fired) {
            mk->click_count = 0;
            return;
        }

        uint64_t duration = mk->release_time_ms - mk->press_time_ms;
        if (duration >= (uint64_t)g_config.long_press_ms) {
            mk->long_press_fired = 1;
            mk->click_count = 0;
            trigger_gesture_action(mk, "Long Press", mk->cfg->long_press, mk->cfg->screen_off_long);
            return;
        }

        mk->click_count++;

        if (mk->click_count == 1) {
            int has_double = mk->cfg && mk->cfg->double_press[0] && strcasecmp(mk->cfg->double_press, "none") != 0;
            int has_triple = mk->cfg && mk->cfg->triple_press[0] && strcasecmp(mk->cfg->triple_press, "none") != 0;
            if (!has_double && !has_triple) {
                // Zero latency: If neither double nor triple click is mapped, trigger single press immediately!
                mk->click_count = 0;
                trigger_gesture_action(mk, "Single Press", mk->cfg ? mk->cfg->single_press : "default", mk->cfg ? mk->cfg->screen_off_single : "default");
            } else {
                set_key_timer(mk, g_config.double_click_ms);
            }
        } else if (mk->click_count == 2) {
            if (mk->cfg->triple_press[0] && strcasecmp(mk->cfg->triple_press, "none") != 0) {
                set_key_timer(mk, g_config.triple_click_ms - g_config.double_click_ms);
            } else {
                disarm_key_timer(mk);
                mk->click_count = 0;
                trigger_gesture_action(mk, "Double Press", mk->cfg->double_press, mk->cfg->screen_off_double);
            }
        } else if (mk->click_count >= 3) {
            disarm_key_timer(mk);
            mk->click_count = 0;
            trigger_gesture_action(mk, "Triple Press", mk->cfg->triple_press, mk->cfg->triple_press);
        }
    }
}

static void handle_timer_expiration(monitored_key_t *mk) {
    uint64_t expirations = 0;
    read(mk->timer_fd, &expirations, sizeof(expirations));

    if (mk->is_down) {
        if (!mk->long_press_fired) {
            mk->long_press_fired = 1;
            mk->click_count = 0;
            trigger_gesture_action(mk, "Long Press (Hold)", mk->cfg->long_press, mk->cfg->screen_off_long);
        }
    } else {
        if (mk->click_count == 1) {
            mk->click_count = 0;
            trigger_gesture_action(mk, "Single Press", mk->cfg->single_press, mk->cfg->screen_off_single);
        } else if (mk->click_count == 2) {
            mk->click_count = 0;
            trigger_gesture_action(mk, "Double Press", mk->cfg->double_press, mk->cfg->screen_off_double);
        }
        mk->click_count = 0;
    }
}

static void sync_monitored_keys_with_config(int epoll_fd) {
    // 1. Auto-discover all hardware keys present on this device
    detected_key_t detected[MAX_DETECTED_KEYS];
    int detected_count = device_detect_all_keys(detected, MAX_DETECTED_KEYS);
    for (int d = 0; d < detected_count; d++) {
        int code = detected[d].keycode;
        key_config_t *kc = config_find_key(&g_config, code);
        if (!kc) {
            kc = config_find_or_add_key(&g_config, code);
            if (kc) {
                strncpy(kc->name, detected[d].name, sizeof(kc->name) - 1);
                strncpy(kc->label, detected[d].label, sizeof(kc->label) - 1);
            }
        }
    }

    // 2. Ensure every configured & detected key has an active monitored entry with timerfd in epoll
    for (int i = 0; i < g_config.key_count; i++) {
        key_config_t *kc = &g_config.keys[i];
        monitored_key_t *mk = find_key_by_code(kc->keycode);
        if (!mk) {
            if (g_monitored_count >= MAX_CONFIG_KEYS) continue;
            mk = &g_monitored_keys[g_monitored_count++];
            memset(mk, 0, sizeof(monitored_key_t));
            mk->keycode = kc->keycode;
            strncpy(mk->name, kc->name, sizeof(mk->name) - 1);
            mk->cfg = kc;

            int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
            if (tfd >= 0) {
                mk->timer_fd = tfd;
                struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = tfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tfd, &ev);
            }
        } else {
            mk->cfg = kc;
        }
    }
}

static const char *resolve_path(const char *primary, const char *fallback, const char *local) {
    if (access(primary, R_OK) == 0) return primary;
    if (access(fallback, R_OK) == 0) return fallback;
    if (access(local, R_OK) == 0) return local;
    return primary;
}

int main(int argc, char *argv[]) {
    int custom_config = 0;
    int custom_webroot = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-t") == 0) {
            g_test_mode = 1;
        } else if (strcmp(argv[i], "--detect-keys") == 0 || strcmp(argv[i], "-d") == 0) {
            device_print_detected_keys_table(stdout);
            return 0;
        } else if (strcmp(argv[i], "--detect-keys-json") == 0) {
            device_print_detected_keys_json(stdout);
            return 0;
        } else if (strcmp(argv[i], "--generate-config") == 0 && i + 1 < argc) {
            const char *out_path = argv[++i];
            printf("[*] Auto-detecting all hardware keys on this device...\n");
            if (config_generate_for_detected_device(out_path)) {
                printf("[+] Successfully generated device-calibrated config: %s\n", out_path);
                return 0;
            } else {
                fprintf(stderr, "[-] Failed to generate config at %s\n", out_path);
                return 1;
            }
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            g_config_path = argv[++i];
            custom_config = 1;
        } else if (strcmp(argv[i], "--webroot") == 0 && i + 1 < argc) {
            g_webroot_path = argv[++i];
            custom_webroot = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: keyactiond [OPTIONS]\n"
                   "  -t, --test                  Run in foreground test mode with live event logging\n"
                   "  -d, --detect-keys           Auto-detect all hardware keys on this device and print table\n"
                   "      --detect-keys-json      Auto-detect all hardware keys and output JSON\n"
                   "      --generate-config <path> Auto-detect all keys and generate tailored config file\n"
                   "  -c, --config <file>         Specify custom path to keymapper.conf\n"
                   "      --webroot <dir>         Specify path to WebUI webroot\n"
                   "  -h, --help                  Show this help message\n");
            return 0;
        }
    }

    printf("[*] Starting Universal KeyMapper Daemon...\n");
    actions_init();

    if (!custom_config) {
        g_config_path = resolve_path(DEFAULT_CONFIG_PATH, FALLBACK_CONFIG_PATH, LOCAL_CONFIG_PATH);
    }
    if (!custom_webroot) {
        g_webroot_path = resolve_path(DEFAULT_WEBROOT_PATH, DEFAULT_WEBROOT_PATH, LOCAL_WEBROOT_PATH);
    }
    g_log_path = resolve_path(DEFAULT_LOG_PATH, DEFAULT_LOG_PATH, LOCAL_LOG_PATH);

    printf("[*] Loading configuration from: %s\n", g_config_path);
    config_load(g_config_path, &g_config);

    // Setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    device_manager_t dm;
    if (device_manager_init(&dm) < 0) {
        fprintf(stderr, "[-] Failed to initialize device manager\n");
        return 1;
    }

    // Auto-detect hardware keys and populate monitored list
    sync_monitored_keys_with_config(dm.epoll_fd);

    device_manager_scan(&dm);
    printf("[+] Initialized %d input devices and %d monitored keys.\n", dm.count, g_monitored_count);

    // Initialize WebUI HTTP server
    int http_fd = -1;
    if (g_config.http_port > 0) {
        http_fd = http_server_init(g_config.http_port);
        if (http_fd >= 0) {
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = http_fd;
            epoll_ctl(dm.epoll_fd, EPOLL_CTL_ADD, http_fd, &ev);
        }
    }

    if (g_test_mode) {
        printf("\033[1;33m[TEST MODE ACTIVE] Press any hardware button on the phone...\033[0m\n");
    }

    struct epoll_event events[32];
    while (g_running) {
        if (g_reload_requested) {
            g_reload_requested = 0;
            printf("[*] Reloading configuration: %s\n", g_config_path);
            config_load(g_config_path, &g_config);
            sync_monitored_keys_with_config(dm.epoll_fd);
        }

        int nfds = epoll_wait(dm.epoll_fd, events, 32, 2000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == dm.inotify_fd) {
                device_manager_handle_inotify(&dm);
                continue;
            }

            if (http_fd >= 0 && fd == http_fd) {
                http_server_handle_client(http_fd, g_webroot_path, g_config_path, g_log_path);
                continue;
            }

            monitored_key_t *timer_key = find_key_by_timer(fd);
            if (timer_key) {
                handle_timer_expiration(timer_key);
                continue;
            }

            struct input_event ev;
            int bytes;
            while ((bytes = read(fd, &ev, sizeof(ev))) == sizeof(ev)) {
                if (ev.type == EV_KEY) {
                    monitored_key_t *mk = find_key_by_code(ev.code);
                    if (mk) {
                        handle_key_event(mk, ev.value);
                    } else {
                        event_record(ev.code, config_get_canonical_name(ev.code), config_get_canonical_label(ev.code), "hardware", ev.value, 0, "unmapped");
                        if (g_test_mode || g_config.log_level >= 1) {
                            printf("[KEY EVENT] %s (%d) | State: %s\n",
                                config_get_canonical_name(ev.code), ev.code,
                                ev.value == 1 ? "DOWN" : (ev.value == 0 ? "UP" : "REPEAT"));
                            fflush(stdout);
                        }
                    }
                }
            }
        }
    }

    printf("[*] Shutting down Universal KeyMapper Daemon...\n");
    if (http_fd >= 0) close(http_fd);
    for (int i = 0; i < g_monitored_count; i++) {
        if (g_monitored_keys[i].timer_fd >= 0) {
            close(g_monitored_keys[i].timer_fd);
        }
    }
    device_manager_close_all(&dm);
    return 0;
}
