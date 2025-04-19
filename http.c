#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define BASE_DIRECTORY "/home/bar0n/Documents/hp/trashttp"

SSL_CTX *init_ssl_context(const char *cert_file, const char *key_file) {
    if (!cert_file || !key_file) {
        return NULL;
    }

    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        perror("Unable to create SSL context");
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        perror("Unable to load certificate or private key");
        exit(EXIT_FAILURE);
    }

    return ctx;
}

int parse_http_request(const char *buffer, http_request_t *request) {
    sscanf(buffer, "%15s %255s %15s", request->method, request->path, request->version);
    return 0;
}

void handle_http1_request(int client_fd, SSL *ssl, const http_request_t *request) {
    char response[BUFFER_SIZE];
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", BASE_DIRECTORY, request->path);

    if (strcmp(request->path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", BASE_DIRECTORY);
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        FILE *file = fopen(file_path, "r");
        if (file) {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %ld\r\n"
                     "Connection: close\r\n\r\n",
                     file_stat.st_size);
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

