#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>

typedef struct {
    int keycode;
    char name[64];
    char label[64];
    char device[128];
    int state; // 0=UP, 1=DOWN, 2=REPEAT
    uint64_t timestamp_ms;
    uint64_t duration_ms;
    char last_action[128];
} live_event_t;

extern live_event_t g_last_event;

void event_record(int keycode, const char *name, const char *label, const char *device, int state, uint64_t duration_ms, const char *action);
void event_to_json(char *buf, size_t buf_len);

#endif // EVENTS_H
