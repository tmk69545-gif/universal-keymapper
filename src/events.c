#include "events.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

live_event_t g_last_event;

void event_record(int keycode, const char *name, const char *label, const char *device, int state, uint64_t duration_ms, const char *action) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;

    g_last_event.keycode = keycode;
    strncpy(g_last_event.name, name ? name : "UNKNOWN", sizeof(g_last_event.name) - 1);
    strncpy(g_last_event.label, label ? label : "Unknown Key", sizeof(g_last_event.label) - 1);
    strncpy(g_last_event.device, device ? device : "input", sizeof(g_last_event.device) - 1);
    g_last_event.state = state;
    g_last_event.timestamp_ms = now;
    g_last_event.duration_ms = duration_ms;
    if (action) {
        strncpy(g_last_event.last_action, action, sizeof(g_last_event.last_action) - 1);
    }
}

void event_to_json(char *buf, size_t buf_len) {
    const char *state_str = (g_last_event.state == 1) ? "DOWN" : ((g_last_event.state == 0) ? "UP" : "REPEAT");
    snprintf(buf, buf_len,
        "{\"keycode\":%d,\"name\":\"%s\",\"label\":\"%s\",\"device\":\"%s\",\"state\":\"%s\",\"value\":%d,\"timestamp\":%llu,\"duration_ms\":%llu,\"action\":\"%s\"}\n",
        g_last_event.keycode,
        g_last_event.name,
        g_last_event.label,
        g_last_event.device,
        state_str,
        g_last_event.state,
        (unsigned long long)g_last_event.timestamp_ms,
        (unsigned long long)g_last_event.duration_ms,
        g_last_event.last_action
    );
}
