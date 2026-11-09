#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define MAX_ACTION_LEN 256
#define MAX_CONFIG_KEYS 64
#define MAX_KEY_NAME_LEN 64
#define MAX_KEY_LABEL_LEN 64

typedef struct {
    int keycode;
    char name[MAX_KEY_NAME_LEN];
    char label[MAX_KEY_LABEL_LEN];
    char single_press[MAX_ACTION_LEN];
    char double_press[MAX_ACTION_LEN];
    char triple_press[MAX_ACTION_LEN];
    char long_press[MAX_ACTION_LEN];
    char screen_off_single[MAX_ACTION_LEN];
    char screen_off_double[MAX_ACTION_LEN];
    char screen_off_long[MAX_ACTION_LEN];
} key_config_t;

typedef struct {
    int double_click_ms;
    int triple_click_ms;
    int long_press_ms;
    int log_level; // 0=Quiet, 1=Info, 2=Debug
    int http_port; // default 8321

    key_config_t keys[MAX_CONFIG_KEYS];
    int key_count;

    // Combos
    char combo_volup_voldown[MAX_ACTION_LEN];
    char combo_power_volup[MAX_ACTION_LEN];
    char combo_power_voldown[MAX_ACTION_LEN];
    char combo_essential_voldown[MAX_ACTION_LEN];
    char combo_essential_volup[MAX_ACTION_LEN];
} app_config_t;

extern app_config_t g_config;

const char *config_get_canonical_name(int keycode);
const char *config_get_canonical_label(int keycode);
key_config_t *config_find_or_add_key(app_config_t *cfg, int keycode);
key_config_t *config_find_key(app_config_t *cfg, int keycode);

void config_set_defaults(app_config_t *cfg);
int config_load(const char *path, app_config_t *cfg);
int config_save(const char *path, const app_config_t *cfg);
int config_generate_for_detected_device(const char *path);

#endif // CONFIG_H
