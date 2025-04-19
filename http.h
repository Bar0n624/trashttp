#ifndef HTTP_H
#define HTTP_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define MAX_HEADERS 100

typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char headers[MAX_HEADERS][2][256];
    char body[BUFFER_SIZE];
} http_request_t;

typedef struct {
    int status_code;
    char headers[MAX_HEADERS][2][256];
    char body[BUFFER_SIZE];
} http_response_t;

SSL_CTX *init_ssl_context(const char *cert_file, const char *key_file);

int parse_http_request(const char *buffer, http_request_t *request);

void handle_http1_request(int client_fd, SSL *ssl, const http_request_t *request);


#endif