#include "device.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

int device_manager_init(device_manager_t *dm) {
    memset(dm, 0, sizeof(device_manager_t));
    dm->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (dm->epoll_fd < 0) {
        perror("epoll_create1");
        return -1;
    }

    dm->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (dm->inotify_fd >= 0) {
        inotify_add_watch(dm->inotify_fd, "/dev/input", IN_CREATE | IN_DELETE);
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = dm->inotify_fd;
        epoll_ctl(dm->epoll_fd, EPOLL_CTL_ADD, dm->inotify_fd, &ev);
    }

    return 0;
}

static int is_already_opened(device_manager_t *dm, const char *path) {
    for (int i = 0; i < dm->count; i++) {
        if (strcmp(dm->devs[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void try_add_device(device_manager_t *dm, const char *path) {
    if (dm->count >= MAX_INPUT_DEVICES) return;
    if (is_already_opened(dm, path)) return;

    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return;

    unsigned long ev_bits[NBITS(EV_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        close(fd);
        return;
    }

    if (!test_bit(EV_KEY, ev_bits)) {
        close(fd);
        return;
    }

    char name[128] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(dm->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        return;
    }

    input_dev_t *dev = &dm->devs[dm->count++];
    dev->fd = fd;
    strncpy(dev->path, path, sizeof(dev->path) - 1);
    strncpy(dev->name, name, sizeof(dev->name) - 1);

    printf("[+] Added input device: %s (\"%s\", fd=%d)\n", path, name, fd);
}

void device_manager_scan(device_manager_t *dm) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            char path[64];
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            try_add_device(dm, path);
        }
    }
    closedir(dir);
}

void device_manager_handle_inotify(device_manager_t *dm) {
    char buf[512];
    int len;
    while ((len = read(dm->inotify_fd, buf, sizeof(buf))) > 0) {
        int i = 0;
        while (i < len) {
            struct inotify_event *event = (struct inotify_event *)&buf[i];
            if (event->len > 0 && strncmp(event->name, "event", 5) == 0) {
                char path[64];
                snprintf(path, sizeof(path), "/dev/input/%s", event->name);
                if (event->mask & IN_CREATE) {
                    try_add_device(dm, path);
                } else if (event->mask & IN_DELETE) {
                    for (int j = 0; j < dm->count; j++) {
                        if (strcmp(dm->devs[j].path, path) == 0) {
                            epoll_ctl(dm->epoll_fd, EPOLL_CTL_DEL, dm->devs[j].fd, NULL);
                            close(dm->devs[j].fd);
                            printf("[-] Removed input device: %s\n", path);
                            dm->devs[j] = dm->devs[--dm->count];
                            break;
                        }
                    }
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }
}

void device_manager_close_all(device_manager_t *dm) {
    for (int i = 0; i < dm->count; i++) {
        if (dm->devs[i].fd >= 0) {
            close(dm->devs[i].fd);
        }
    }
    dm->count = 0;
    if (dm->inotify_fd >= 0) {
        close(dm->inotify_fd);
        dm->inotify_fd = -1;
    }
    if (dm->epoll_fd >= 0) {
        close(dm->epoll_fd);
        dm->epoll_fd = -1;
    }
}

int device_detect_all_keys(detected_key_t *out_keys, int max_keys) {
    int total = 0;
    DIR *dir = opendir("/dev/input");
    if (!dir) return 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char name[128] = "Unknown";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        unsigned long ev_bits[NBITS(EV_MAX)] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0 || !test_bit(EV_KEY, ev_bits)) {
            close(fd);
            continue;
        }

        unsigned long key_bits[NBITS(KEY_MAX)] = {0};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);

        // Check if device is a touchscreen (has ABS_MT)
        unsigned long abs_bits[NBITS(ABS_MAX)] = {0};
        int is_touchscreen = 0;
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) >= 0) {
            if (test_bit(ABS_MT_SLOT, abs_bits) || test_bit(ABS_MT_POSITION_X, abs_bits)) {
                is_touchscreen = 1;
            }
        }

        for (int code = 1; code < KEY_MAX; code++) {
            if (!test_bit(code, key_bits)) continue;

            // Filter out touch/mouse coordinates
            if (code >= BTN_MISC && code <= BTN_MOUSE) continue;
            if (code == BTN_TOUCH) continue;

            // If it's a touchscreen, only take dedicated virtual buttons or power/assist
            if (is_touchscreen) {
                if (code != KEY_POWER && code != 250 && code != KEY_VOLUMEUP && code != KEY_VOLUMEDOWN) {
                    continue;
                }
            }

            // Check if already in out_keys
            int exists = 0;
            for (int k = 0; k < total; k++) {
                if (out_keys[k].keycode == code) {
                    exists = 1;
                    break;
                }
            }
            if (exists) continue;

            if (total < max_keys) {
                detected_key_t *dk = &out_keys[total++];
                dk->keycode = code;
                strncpy(dk->name, config_get_canonical_name(code), sizeof(dk->name) - 1);
                strncpy(dk->label, config_get_canonical_label(code), sizeof(dk->label) - 1);
                strncpy(dk->device_name, name, sizeof(dk->device_name) - 1);
                strncpy(dk->device_path, path, sizeof(dk->device_path) - 1);
            }
        }
        close(fd);
    }
    closedir(dir);
    return total;
}

void device_print_detected_keys_json(FILE *out) {
    detected_key_t keys[MAX_DETECTED_KEYS];
    int count = device_detect_all_keys(keys, MAX_DETECTED_KEYS);

    fprintf(out, "[\n");
    for (int i = 0; i < count; i++) {
        fprintf(out, "  {\n");
        fprintf(out, "    \"keycode\": %d,\n", keys[i].keycode);
        fprintf(out, "    \"name\": \"%s\",\n", keys[i].name);
        fprintf(out, "    \"label\": \"%s\",\n", keys[i].label);
        fprintf(out, "    \"device\": \"%s\",\n", keys[i].device_name);
        fprintf(out, "    \"path\": \"%s\"\n", keys[i].device_path);
        fprintf(out, "  }%s\n", (i + 1 < count) ? "," : "");
    }
    fprintf(out, "]\n");
}

void device_print_detected_keys_table(FILE *out) {
    detected_key_t keys[MAX_DETECTED_KEYS];
    int count = device_detect_all_keys(keys, MAX_DETECTED_KEYS);

    fprintf(out, "+---------+-----------------------+--------------------------+-----------------------+\n");
    fprintf(out, "| Keycode | Key Name              | Friendly Label           | Hardware Device       |\n");
    fprintf(out, "+---------+-----------------------+--------------------------+-----------------------+\n");
    for (int i = 0; i < count; i++) {
        fprintf(out, "| %-7d | %-21s | %-24s | %-21s |\n",
            keys[i].keycode, keys[i].name, keys[i].label, keys[i].device_name);
    }
    fprintf(out, "+---------+-----------------------+--------------------------+-----------------------+\n");
    fprintf(out, "Total detected keys: %d\n", count);
}
