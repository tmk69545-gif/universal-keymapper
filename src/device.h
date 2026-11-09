#ifndef DEVICE_H
#define DEVICE_H

#include <stdio.h>
#include <stdint.h>

#define MAX_INPUT_DEVICES 32
#define MAX_DETECTED_KEYS 128

typedef struct {
    int fd;
    char path[64];
    char name[128];
} input_dev_t;

typedef struct {
    input_dev_t devs[MAX_INPUT_DEVICES];
    int count;
    int epoll_fd;
    int inotify_fd;
} device_manager_t;

typedef struct {
    int keycode;
    char name[64];
    char label[64];
    char device_name[128];
    char device_path[64];
} detected_key_t;

int device_manager_init(device_manager_t *dm);
void device_manager_scan(device_manager_t *dm);
void device_manager_handle_inotify(device_manager_t *dm);
void device_manager_close_all(device_manager_t *dm);

// Hardware auto-detection
int device_detect_all_keys(detected_key_t *out_keys, int max_keys);
void device_print_detected_keys_json(FILE *out);
void device_print_detected_keys_table(FILE *out);

#endif // DEVICE_H
