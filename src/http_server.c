#include "http_server.h"
#include "device.h"
#include "config.h"
#include "actions.h"
#include "events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

extern volatile sig_atomic_t g_reload_requested;

int http_server_init(int port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to 0.0.0.0 or 127.0.0.1
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind http_server");
        close(fd);
        return -1;
    }

    if (listen(fd, 8) < 0) {
        perror("listen http_server");
        close(fd);
        return -1;
    }

    printf("[+] WebUI HTTP server listening on http://127.0.0.1:%d\n", port);
    return fd;
}

static void send_response(int client_fd, int code, const char *status, const char *content_type, const char *body, size_t body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        code, status, content_type, body_len
    );
    write(client_fd, header, hlen);
    if (body && body_len > 0) {
        write(client_fd, body, body_len);
    }
}

static void serve_file(int client_fd, const char *filepath, const char *content_type) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send_response(client_fd, 404, "Not Found", "text/plain", "404 Not Found", 13);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, fp);
        send_response(client_fd, 200, "OK", content_type, buf, size);
        free(buf);
    } else {
        send_response(client_fd, 500, "Internal Error", "text/plain", "OOM", 3);
    }
    fclose(fp);
}

void http_server_handle_client(int server_fd, const char *webroot_path, const char *config_path, const char *log_path) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr, &client_len, SOCK_CLOEXEC);
    if (client_fd < 0) return;

    // Set 3s socket read/write timeout so client cannot hang daemon
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    int max_buf = 65536;
    char *req = malloc(max_buf);
    if (!req) {
        close(client_fd);
        return;
    }

    int total_read = 0;
    char *hdr_end = NULL;

    // Read until headers \r\n\r\n are completely received
    while (total_read < max_buf - 1) {
        int r = read(client_fd, req + total_read, max_buf - 1 - total_read);
        if (r <= 0) break;
        total_read += r;
        req[total_read] = '\0';
        hdr_end = strstr(req, "\r\n\r\n");
        if (hdr_end) break;
    }

    if (!hdr_end) {
        free(req);
        close(client_fd);
        return;
    }

    int header_len = (int)(hdr_end - req) + 4;
    int content_length = 0;
    char *cl = strcasestr(req, "Content-Length:");
    if (cl) {
        content_length = atoi(cl + 15);
    }

    // Read remaining body bytes if Content-Length specified
    if (content_length > 0) {
        int target = header_len + content_length;
        if (target >= max_buf) {
            send_response(client_fd, 413, "Payload Too Large", "text/plain", "Payload Too Large\n", 18);
            free(req);
            close(client_fd);
            return;
        }
        while (total_read < target) {
            int r = read(client_fd, req + total_read, target - total_read);
            if (r <= 0) break;
            total_read += r;
            req[total_read] = '\0';
        }
    }
    req[total_read] = '\0';

    char method[16], uri[256];
    if (sscanf(req, "%15s %255s", method, uri) != 2) {
        free(req);
        close(client_fd);
        return;
    }

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_fd, 204, "No Content", "text/plain", "", 0);
        free(req);
        close(client_fd);
        return;
    }

    char *body = hdr_end + 4;

    // API: Status
    if (strcmp(uri, "/api/status") == 0) {
        char json[256];
        int jlen = snprintf(json, sizeof(json), "{\"running\":true,\"pid\":%d,\"port\":%d,\"keys_count\":%d}\n",
            getpid(), g_config.http_port, g_config.key_count);
        send_response(client_fd, 200, "OK", "application/json", json, jlen);
        free(req);
        close(client_fd);
        return;
    }

    // API: Live Key Event
    if (strcmp(uri, "/api/events") == 0) {
        char json[512];
        event_to_json(json, sizeof(json));
        send_response(client_fd, 200, "OK", "application/json", json, strlen(json));
        free(req);
        close(client_fd);
        return;
    }

    // API: Keys
    if (strcmp(uri, "/api/keys") == 0) {
        detected_key_t detected[MAX_DETECTED_KEYS];
        int count = device_detect_all_keys(detected, MAX_DETECTED_KEYS);
        char *json = malloc(16384);
        if (json) {
            int pos = snprintf(json, 16384, "[\n");
            for (int i = 0; i < count; i++) {
                pos += snprintf(json + pos, 16384 - pos,
                    "  {\"keycode\":%d,\"name\":\"%s\",\"label\":\"%s\",\"device\":\"%s\",\"path\":\"%s\"}%s\n",
                    detected[i].keycode, detected[i].name, detected[i].label,
                    detected[i].device_name, detected[i].device_path,
                    (i + 1 < count) ? "," : "");
            }
            pos += snprintf(json + pos, 16384 - pos, "]\n");
            send_response(client_fd, 200, "OK", "application/json", json, pos);
            free(json);
        }
        free(req);
        close(client_fd);
        return;
    }

    // API: Logs (GET)
    if (strcmp(uri, "/api/logs") == 0 && strcmp(method, "GET") == 0) {
        FILE *fp = fopen(log_path, "r");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            long offset = (sz > 32768) ? (sz - 32768) : 0;
            fseek(fp, offset, SEEK_SET);

            char *buf = malloc(32768 + 1);
            if (buf) {
                size_t rd = fread(buf, 1, 32768, fp);
                buf[rd] = '\0';
                send_response(client_fd, 200, "OK", "text/plain", buf, rd);
                free(buf);
            }
            fclose(fp);
        } else {
            send_response(client_fd, 200, "OK", "text/plain", "No logs recorded yet.\n", 22);
        }
        free(req);
        close(client_fd);
        return;
    }

    // API: Logs (DELETE or POST /api/logs/clear)
    if ((strcmp(uri, "/api/logs") == 0 && strcmp(method, "DELETE") == 0) ||
        strcmp(uri, "/api/logs/clear") == 0) {
        FILE *fp = fopen(log_path, "w");
        if (fp) fclose(fp);
        send_response(client_fd, 200, "OK", "application/json", "{\"cleared\":true}\n", 17);
        free(req);
        close(client_fd);
        return;
    }

    // API: Config (GET)
    if (strcmp(uri, "/api/config") == 0 && strcmp(method, "GET") == 0) {
        serve_file(client_fd, config_path, "text/plain");
        free(req);
        close(client_fd);
        return;
    }

    // API: Config (POST) - with atomic write and empty body protection
    if (strcmp(uri, "/api/config") == 0 && strcmp(method, "POST") == 0) {
        if (!body || strlen(body) < 10) {
            send_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"empty or truncated config body\"}\n", 43);
            free(req);
            close(client_fd);
            return;
        }

        char tmp_path[512];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", config_path);
        FILE *fp = fopen(tmp_path, "w");
        if (fp) {
            size_t body_len = strlen(body);
            size_t written = fwrite(body, 1, body_len, fp);
            fclose(fp);
            if (written == body_len) {
                rename(tmp_path, config_path);
                g_reload_requested = 1;
                printf("[CONFIG] Configuration updated successfully (%zu bytes). Reload triggered.\n", written);
                fflush(stdout);
                send_response(client_fd, 200, "OK", "application/json", "{\"saved\":true,\"reloaded\":true}\n", 30);
            } else {
                unlink(tmp_path);
                send_response(client_fd, 500, "Error", "application/json", "{\"error\":\"write incomplete\"}\n", 28);
            }
        } else {
            send_response(client_fd, 500, "Error", "application/json", "{\"error\":\"write failed\"}\n", 24);
        }
        free(req);
        close(client_fd);
        return;
    }

    // API: Reload
    if (strcmp(uri, "/api/reload") == 0) {
        g_reload_requested = 1;
        printf("[CONFIG] Manual reload requested via API.\n");
        fflush(stdout);
        send_response(client_fd, 200, "OK", "application/json", "{\"reloaded\":true}\n", 18);
        free(req);
        close(client_fd);
        return;
    }

    // API: Test Action
    if (strcmp(uri, "/api/test-action") == 0 && strcmp(method, "POST") == 0) {
        char *act = body;
        while (*act == ' ' || *act == '\t' || *act == '\r' || *act == '\n') act++;
        char *end = act + strlen(act) - 1;
        while (end >= act && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }

        if (*act) {
            printf("[API TEST] Triggering action: '%s'\n", act);
            fflush(stdout);
            actions_execute(act);
            send_response(client_fd, 200, "OK", "application/json", "{\"tested\":true}\n", 16);
        } else {
            send_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"no action specified\"}\n", 32);
        }
        free(req);
        close(client_fd);
        return;
    }

    // Static Webroot files
    char filepath[512];
    if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", webroot_path);
        serve_file(client_fd, filepath, "text/html");
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", webroot_path, uri);
        const char *mime = "text/plain";
        if (strstr(uri, ".html")) mime = "text/html";
        else if (strstr(uri, ".css")) mime = "text/css";
        else if (strstr(uri, ".js")) mime = "application/javascript";
        else if (strstr(uri, ".json")) mime = "application/json";
        else if (strstr(uri, ".png")) mime = "image/png";
        else if (strstr(uri, ".svg")) mime = "image/svg+xml";
        serve_file(client_fd, filepath, mime);
    }

    free(req);
    close(client_fd);
}
