#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>

int http_server_init(int port);
void http_server_handle_client(int server_fd, const char *webroot_path, const char *config_path, const char *log_path);

#endif // HTTP_SERVER_H
