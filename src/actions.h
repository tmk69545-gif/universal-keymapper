#ifndef ACTIONS_H
#define ACTIONS_H

#include <stdint.h>

#define MAX_ACTION_LEN 256

void actions_init(void);
int actions_is_screen_on(void);
void actions_execute(const char *action_str);

void actions_toggle_flashlight(void);
void actions_screenshot(void);
void actions_play_pause(void);
void actions_next_track(void);
void actions_prev_track(void);
void actions_volume_up(void);
void actions_volume_down(void);
void actions_mute_toggle(void);
void actions_screen_off(void);
void actions_screen_on(void);
void actions_camera(void);
void actions_notifications(void);
void actions_quick_settings(void);
void actions_kill_foreground_app(void);
void actions_run_shell(const char *cmd);

#endif // ACTIONS_H
