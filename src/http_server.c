#include "http_server.h"
#include "device.h"
#include "config.h"
#include "actions.h"
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

    char req[4096];
    int n = read(client_fd, req, sizeof(req) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    req[n] = '\0';

    char method[16], uri[256];
    if (sscanf(req, "%15s %255s", method, uri) != 2) {
        close(client_fd);
        return;
    }

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_fd, 204, "No Content", "text/plain", "", 0);
        close(client_fd);
        return;
    }

    // API: Status
    if (strcmp(uri, "/api/status") == 0) {
        char json[256];
        int jlen = snprintf(json, sizeof(json), "{\"running\":true,\"pid\":%d,\"port\":%d,\"keys_count\":%d}\n",
            getpid(), g_config.http_port, g_config.key_count);
        send_response(client_fd, 200, "OK", "application/json", json, jlen);
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
        close(client_fd);
        return;
    }

    // API: Logs (DELETE or POST /api/logs/clear)
    if ((strcmp(uri, "/api/logs") == 0 && strcmp(method, "DELETE") == 0) ||
        strcmp(uri, "/api/logs/clear") == 0) {
        FILE *fp = fopen(log_path, "w");
        if (fp) fclose(fp);
        send_response(client_fd, 200, "OK", "application/json", "{\"cleared\":true}\n", 17);
        close(client_fd);
        return;
    }

    // API: Config (GET)
    if (strcmp(uri, "/api/config") == 0 && strcmp(method, "GET") == 0) {
        serve_file(client_fd, config_path, "text/plain");
        close(client_fd);
        return;
    }

    // API: Config (POST)
    if (strcmp(uri, "/api/config") == 0 && strcmp(method, "POST") == 0) {
        char *body = strstr(req, "\r\n\r\n");
        if (body) {
            body += 4;
            FILE *fp = fopen(config_path, "w");
            if (fp) {
                fputs(body, fp);
                fclose(fp);
                g_reload_requested = 1;
                send_response(client_fd, 200, "OK", "application/json", "{\"saved\":true,\"reloaded\":true}\n", 30);
            } else {
                send_response(client_fd, 500, "Error", "application/json", "{\"error\":\"write failed\"}\n", 24);
            }
        }
        close(client_fd);
        return;
    }

    // API: Reload
    if (strcmp(uri, "/api/reload") == 0) {
        g_reload_requested = 1;
        send_response(client_fd, 200, "OK", "application/json", "{\"reloaded\":true}\n", 18);
        close(client_fd);
        return;
    }

    // API: Test Action
    if (strcmp(uri, "/api/test-action") == 0 && strcmp(method, "POST") == 0) {
        char *body = strstr(req, "\r\n\r\n");
        if (body) {
            body += 4;
            actions_execute(body);
            send_response(client_fd, 200, "OK", "application/json", "{\"tested\":true}\n", 16);
        } else {
            send_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"no body\"}\n", 21);
        }
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

    close(client_fd);
}
