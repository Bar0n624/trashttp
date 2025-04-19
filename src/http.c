#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

const char* get_mime_type(const char* file_path) {
    const char* dot = strrchr(file_path, '.');
    if (!dot || dot == file_path) return "application/octet-stream";

    char ext[32] = {0};
    strncpy(ext, dot + 1, sizeof(ext) - 1);
    for (int i = 0; ext[i]; i++) {
        ext[i] = tolower(ext[i]);
    }

    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcmp(ext, "ico") == 0) return "image/x-icon";
    if (strcmp(ext, "pdf") == 0) return "application/pdf";
    if (strcmp(ext, "txt") == 0) return "text/plain";
    if (strcmp(ext, "xml") == 0) return "application/xml";
    if (strcmp(ext, "woff") == 0) return "font/woff";
    if (strcmp(ext, "woff2") == 0) return "font/woff2";
    if (strcmp(ext, "ttf") == 0) return "font/ttf";

    return "application/octet-stream";
}

int parse_http_request(const char *buffer, http_request_t *request) {
    sscanf(buffer, "%15s %255s %15s", request->method, request->path, request->version);
    return 0;
}

void handle_http1_request(int client_fd, SSL *ssl, const http_request_t *request, const char *base_directory) {
    char response[BUFFER_SIZE];
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", base_directory, request->path);

    if (strcmp(request->path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", base_directory);
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        FILE *file = fopen(file_path, "r");
        if (file) {

            const char* mime_type = get_mime_type(file_path);

            int is_binary = (strncmp(mime_type, "image/", 6) == 0 ||
                             strncmp(mime_type, "application/", 12) == 0 ||
                             strncmp(mime_type, "font/", 5) == 0);

            if (is_binary) {
                fclose(file);
                file = fopen(file_path, "rb");
                if (!file) {
                    const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                    if (ssl) {
                        SSL_write(ssl, error_response, strlen(error_response));
                    } else {
                        send(client_fd, error_response, strlen(error_response), 0);
                    }
                    close(client_fd);
                    return;
                }
            }

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %ld\r\n"
                     "Connection: close\r\n\r\n",
                     mime_type, file_stat.st_size);

            if (ssl) {
                SSL_write(ssl, response, strlen(response));
            } else {
                send(client_fd, response, strlen(response), 0);
            }

            char file_buffer[BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                if (ssl) {
                    SSL_write(ssl, file_buffer, bytes_read);
                } else {
                    send(client_fd, file_buffer, bytes_read, 0);
                }
            }
            fclose(file);
        } else {
            const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            if (ssl) {
                SSL_write(ssl, error_response, strlen(error_response));
            } else {
                send(client_fd, error_response, strlen(error_response), 0);
            }
        }
    } else {
        const char *not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                         "Content-Type: text/html\r\n"
                                         "Connection: close\r\n\r\n"
                                         "<html><body><h1>404 Not Found</h1></body></html>";
        if (ssl) {
            SSL_write(ssl, not_found_response, strlen(not_found_response));
        } else {
            send(client_fd, not_found_response, strlen(not_found_response), 0);
        }
    }

    close(client_fd);
}