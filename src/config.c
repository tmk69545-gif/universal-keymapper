#include "config.h"
#include "device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

app_config_t g_config;

static char *trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

const char *config_get_canonical_name(int keycode) {
    switch (keycode) {
        case 114: return "KEY_VOLUMEDOWN";
        case 115: return "KEY_VOLUMEUP";
        case 116: return "KEY_POWER";
        case 113: return "KEY_MUTE";
        case 250: return "KEY_ESSENTIAL";
        case 212: return "KEY_CAMERA";
        case 528: return "KEY_CAMERA_FOCUS";
        case 246: return "KEY_VOICECOMMAND";
        case 583: return "KEY_ASSISTANT";
        case 703: return "KEY_BIXBY";
        case 103: return "KEY_UP";
        case 108: return "KEY_DOWN";
        case 105: return "KEY_LEFT";
        case 106: return "KEY_RIGHT";
        case 158: return "KEY_BACK";
        case 172: return "KEY_HOMEPAGE";
        case 127: return "KEY_MENU";
        case 217: return "KEY_SEARCH";
        case 164: return "KEY_PLAYPAUSE";
        case 163: return "KEY_NEXTSONG";
        case 165: return "KEY_PREVIOUSSONG";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "KEY_%d", keycode);
            return buf;
        }
    }
}

const char *config_get_canonical_label(int keycode) {
    switch (keycode) {
        case 114: return "Volume Down";
        case 115: return "Volume Up";
        case 116: return "Power Button";
        case 113: return "Mute Switch";
        case 250: return "Essential / Action Key";
        case 212: return "Camera Shutter";
        case 528: return "Camera Focus";
        case 246: return "Voice Assistant";
        case 583: return "Assistant Key";
        case 703: return "Bixby / AI Key";
        case 103: return "D-Pad Up";
        case 108: return "D-Pad Down";
        case 105: return "D-Pad Left";
        case 106: return "D-Pad Right";
        case 158: return "Back Button";
        case 172: return "Home Button";
        case 127: return "Menu Button";
        case 217: return "Search Button";
        case 164: return "Play / Pause";
        case 163: return "Next Track";
        case 165: return "Previous Track";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "Hardware Key %d", keycode);
            return buf;
        }
    }
}

key_config_t *config_find_key(app_config_t *cfg, int keycode) {
    for (int i = 0; i < cfg->key_count; i++) {
        if (cfg->keys[i].keycode == keycode) {
            return &cfg->keys[i];
        }
    }
    return NULL;
}

key_config_t *config_find_or_add_key(app_config_t *cfg, int keycode) {
    key_config_t *found = config_find_key(cfg, keycode);
    if (found) return found;

    if (cfg->key_count >= MAX_CONFIG_KEYS) return NULL;
    key_config_t *kc = &cfg->keys[cfg->key_count++];
    memset(kc, 0, sizeof(key_config_t));
    kc->keycode = keycode;
    strncpy(kc->name, config_get_canonical_name(keycode), sizeof(kc->name) - 1);
    strncpy(kc->label, config_get_canonical_label(keycode), sizeof(kc->label) - 1);

    strncpy(kc->single_press, "default", MAX_ACTION_LEN - 1);
    strncpy(kc->double_press, "none", MAX_ACTION_LEN - 1);
    strncpy(kc->triple_press, "none", MAX_ACTION_LEN - 1);
    strncpy(kc->long_press, "none", MAX_ACTION_LEN - 1);
    strncpy(kc->screen_off_single, "default", MAX_ACTION_LEN - 1);
    strncpy(kc->screen_off_double, "none", MAX_ACTION_LEN - 1);
    strncpy(kc->screen_off_long, "none", MAX_ACTION_LEN - 1);

    return kc;
}

static int parse_keycode_from_section(const char *section) {
    if (strncasecmp(section, "key_", 4) == 0) {
        const char *sub = section + 4;
        if (strcasecmp(sub, "essential") == 0) return 250;
        if (strcasecmp(sub, "volume_down") == 0 || strcasecmp(sub, "voldown") == 0) return 114;
        if (strcasecmp(sub, "volume_up") == 0 || strcasecmp(sub, "volup") == 0) return 115;
        if (strcasecmp(sub, "power") == 0) return 116;
        if (strcasecmp(sub, "camera") == 0) return 212;
        if (strcasecmp(sub, "assistant") == 0) return 583;
        if (strcasecmp(sub, "bixby") == 0) return 703;
        if (strcasecmp(sub, "mute") == 0) return 113;
        int code = atoi(sub);
        if (code > 0) return code;
    }
    return -1;
}

void config_set_defaults(app_config_t *cfg) {
    memset(cfg, 0, sizeof(app_config_t));
    cfg->double_click_ms = 300;
    cfg->triple_click_ms = 450;
    cfg->long_press_ms = 480;
    cfg->log_level = 1;
    cfg->http_port = 8321;

    // Combos
    strncpy(cfg->combo_volup_voldown, "screenshot", MAX_ACTION_LEN - 1);
    strncpy(cfg->combo_power_volup, "none", MAX_ACTION_LEN - 1);
    strncpy(cfg->combo_power_voldown, "default", MAX_ACTION_LEN - 1);
    strncpy(cfg->combo_essential_voldown, "notifications", MAX_ACTION_LEN - 1);
    strncpy(cfg->combo_essential_volup, "quick_settings", MAX_ACTION_LEN - 1);

    // Essential Key (250)
    key_config_t *k250 = config_find_or_add_key(cfg, 250);
    if (k250) {
        strncpy(k250->single_press, "flashlight", MAX_ACTION_LEN - 1);
        strncpy(k250->double_press, "screenshot", MAX_ACTION_LEN - 1);
        strncpy(k250->long_press, "camera", MAX_ACTION_LEN - 1);
        strncpy(k250->screen_off_single, "flashlight", MAX_ACTION_LEN - 1);
        strncpy(k250->screen_off_double, "play_pause", MAX_ACTION_LEN - 1);
        strncpy(k250->screen_off_long, "flashlight", MAX_ACTION_LEN - 1);
    }

    // Vol Down (114)
    key_config_t *k114 = config_find_or_add_key(cfg, 114);
    if (k114) {
        strncpy(k114->screen_off_long, "prev_track", MAX_ACTION_LEN - 1);
    }

    // Vol Up (115)
    key_config_t *k115 = config_find_or_add_key(cfg, 115);
    if (k115) {
        strncpy(k115->screen_off_long, "next_track", MAX_ACTION_LEN - 1);
    }

    // Power (116)
    key_config_t *k116 = config_find_or_add_key(cfg, 116);
    if (k116) {
        strncpy(k116->double_press, "default", MAX_ACTION_LEN - 1);
        strncpy(k116->long_press, "default", MAX_ACTION_LEN - 1);
        strncpy(k116->screen_off_single, "default", MAX_ACTION_LEN - 1);
        strncpy(k116->screen_off_double, "default", MAX_ACTION_LEN - 1);
        strncpy(k116->screen_off_long, "default", MAX_ACTION_LEN - 1);
    }
}

static void apply_key_setting(key_config_t *kc, const char *key, const char *val) {
    if (strcasecmp(key, "label") == 0) {
        strncpy(kc->label, val, sizeof(kc->label) - 1);
    } else if (strcasecmp(key, "single_press") == 0) {
        strncpy(kc->single_press, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "double_press") == 0) {
        strncpy(kc->double_press, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "triple_press") == 0) {
        strncpy(kc->triple_press, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "long_press") == 0) {
        strncpy(kc->long_press, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "screen_off_single") == 0) {
        strncpy(kc->screen_off_single, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "screen_off_double") == 0) {
        strncpy(kc->screen_off_double, val, MAX_ACTION_LEN - 1);
    } else if (strcasecmp(key, "screen_off_long") == 0 || strcasecmp(key, "screen_off_long_press") == 0) {
        strncpy(kc->screen_off_long, val, MAX_ACTION_LEN - 1);
    }
}

int config_load(const char *path, app_config_t *cfg) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        config_set_defaults(cfg);
        config_save(path, cfg);
        return 0;
    }

    config_set_defaults(cfg);
    char line[512];
    char section[64] = "global";

    while (fgets(line, sizeof(line), fp)) {
        char *p = trim_whitespace(line);
        if (*p == '#' || *p == ';' || *p == '\0') {
            continue;
        }

        if (*p == '[' && strchr(p, ']')) {
            char *end = strchr(p, ']');
            *end = '\0';
            strncpy(section, trim_whitespace(p + 1), sizeof(section) - 1);
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_whitespace(p);
        char *val = trim_whitespace(eq + 1);

        if (strcasecmp(section, "global") == 0) {
            if (strcasecmp(key, "double_click_ms") == 0) {
                cfg->double_click_ms = atoi(val);
            } else if (strcasecmp(key, "triple_click_ms") == 0) {
                cfg->triple_click_ms = atoi(val);
            } else if (strcasecmp(key, "long_press_ms") == 0) {
                cfg->long_press_ms = atoi(val);
            } else if (strcasecmp(key, "log_level") == 0) {
                cfg->log_level = atoi(val);
            } else if (strcasecmp(key, "http_port") == 0) {
                cfg->http_port = atoi(val);
            }
        } else if (strcasecmp(section, "combos") == 0) {
            if (strcasecmp(key, "volup_and_voldown") == 0) {
                strncpy(cfg->combo_volup_voldown, val, MAX_ACTION_LEN - 1);
            } else if (strcasecmp(key, "power_and_volup") == 0) {
                strncpy(cfg->combo_power_volup, val, MAX_ACTION_LEN - 1);
            } else if (strcasecmp(key, "power_and_voldown") == 0) {
                strncpy(cfg->combo_power_voldown, val, MAX_ACTION_LEN - 1);
            } else if (strcasecmp(key, "essential_and_voldown") == 0) {
                strncpy(cfg->combo_essential_voldown, val, MAX_ACTION_LEN - 1);
            } else if (strcasecmp(key, "essential_and_volup") == 0) {
                strncpy(cfg->combo_essential_volup, val, MAX_ACTION_LEN - 1);
            }
        } else {
            int code = parse_keycode_from_section(section);
            if (code > 0) {
                key_config_t *kc = config_find_or_add_key(cfg, code);
                if (kc) {
                    apply_key_setting(kc, key, val);
                }
            }
        }
    }

    fclose(fp);
    return 1;
}

int config_save(const char *path, const app_config_t *cfg) {
    FILE *fp = fopen(path, "w");
    if (!fp) return 0;

    fprintf(fp,
        "# ====================================================================\n"
        "# Universal KeyMapper Configuration File\n"
        "# Path: %s\n"
        "# Changes take effect immediately via WebUI or 'su -c keymapper reload'\n"
        "# ====================================================================\n"
        "# Supported Built-in Actions:\n"
        "#   default         - Let system handle key normally\n"
        "#   none            - Do nothing / ignore event\n"
        "#   flashlight      - Toggle camera flashlight ON / OFF\n"
        "#   screenshot      - Capture screen\n"
        "#   play_pause      - Media Play/Pause toggle\n"
        "#   next_track      - Skip to next media track\n"
        "#   prev_track      - Skip to previous media track\n"
        "#   volume_up       - Increase media volume\n"
        "#   volume_down     - Decrease media volume\n"
        "#   mute_toggle     - Cycle Normal -> Vibrate -> Silent\n"
        "#   screen_off      - Turn screen off / Lock\n"
        "#   screen_on       - Wake screen up\n"
        "#   camera          - Launch default camera app\n"
        "#   notifications   - Expand notification drawer\n"
        "#   quick_settings  - Expand quick settings tiles\n"
        "#   kill_app        - Force kill active foreground app\n"
        "#   shell <cmd>     - Execute arbitrary root shell command\n"
        "# ====================================================================\n\n"
        "[global]\n"
        "double_click_ms = %d\n"
        "triple_click_ms = %d\n"
        "long_press_ms = %d\n"
        "log_level = %d\n"
        "http_port = %d\n\n",
        path,
        cfg->double_click_ms,
        cfg->triple_click_ms,
        cfg->long_press_ms,
        cfg->log_level,
        cfg->http_port
    );

    // Save each key section
    for (int i = 0; i < cfg->key_count; i++) {
        const key_config_t *kc = &cfg->keys[i];
        fprintf(fp,
            "# --------------------------------------------------------------------\n"
            "# Keycode %d: %s (%s)\n"
            "# --------------------------------------------------------------------\n"
            "[key_%d]\n"
            "label = %s\n"
            "single_press = %s\n"
            "double_press = %s\n"
            "triple_press = %s\n"
            "long_press = %s\n"
            "screen_off_single = %s\n"
            "screen_off_double = %s\n"
            "screen_off_long = %s\n\n",
            kc->keycode, kc->name, kc->label,
            kc->keycode,
            kc->label,
            kc->single_press,
            kc->double_press,
            kc->triple_press,
            kc->long_press,
            kc->screen_off_single,
            kc->screen_off_double,
            kc->screen_off_long
        );
    }

    fprintf(fp,
        "# --------------------------------------------------------------------\n"
        "# Button Combinations (Pressed Simultaneously)\n"
        "# --------------------------------------------------------------------\n"
        "[combos]\n"
        "volup_and_voldown = %s\n"
        "essential_and_voldown = %s\n"
        "essential_and_volup = %s\n"
        "power_and_volup = %s\n"
        "power_and_voldown = %s\n",
        cfg->combo_volup_voldown,
        cfg->combo_essential_voldown,
        cfg->combo_essential_volup,
        cfg->combo_power_volup,
        cfg->combo_power_voldown
    );

    fclose(fp);
    return 1;
}

int config_generate_for_detected_device(const char *path) {
    app_config_t cfg;
    config_set_defaults(&cfg);

    detected_key_t detected[MAX_DETECTED_KEYS];
    int count = device_detect_all_keys(detected, MAX_DETECTED_KEYS);

    for (int i = 0; i < count; i++) {
        int code = detected[i].keycode;
        key_config_t *kc = config_find_or_add_key(&cfg, code);
        if (kc) {
            strncpy(kc->name, detected[i].name, sizeof(kc->name) - 1);
            strncpy(kc->label, detected[i].label, sizeof(kc->label) - 1);

            // Apply smart default actions based on key nature
            if (code == 250) { // Essential key
                strncpy(kc->single_press, "flashlight", MAX_ACTION_LEN - 1);
                strncpy(kc->double_press, "screenshot", MAX_ACTION_LEN - 1);
                strncpy(kc->long_press, "camera", MAX_ACTION_LEN - 1);
                strncpy(kc->screen_off_single, "flashlight", MAX_ACTION_LEN - 1);
            } else if (code == 114) { // Vol Down
                strncpy(kc->single_press, "default", MAX_ACTION_LEN - 1);
                strncpy(kc->screen_off_long, "prev_track", MAX_ACTION_LEN - 1);
            } else if (code == 115) { // Vol Up
                strncpy(kc->single_press, "default", MAX_ACTION_LEN - 1);
                strncpy(kc->screen_off_long, "next_track", MAX_ACTION_LEN - 1);
            } else if (code == 116) { // Power
                strncpy(kc->single_press, "default", MAX_ACTION_LEN - 1);
                strncpy(kc->double_press, "default", MAX_ACTION_LEN - 1);
                strncpy(kc->long_press, "default", MAX_ACTION_LEN - 1);
            } else if (code == 212) { // Camera shutter
                strncpy(kc->single_press, "camera", MAX_ACTION_LEN - 1);
                strncpy(kc->long_press, "camera", MAX_ACTION_LEN - 1);
            } else if (code == 583 || code == 703 || code == 246) { // Assistant / Bixby
                strncpy(kc->single_press, "screenshot", MAX_ACTION_LEN - 1);
                strncpy(kc->long_press, "flashlight", MAX_ACTION_LEN - 1);
            }
        }
    }

    return config_save(path, &cfg);
}
