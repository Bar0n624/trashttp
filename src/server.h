#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <openssl/ssl.h>
#include "config.h"
#include "thread_pool.h"

typedef struct {
    int server_fd;
    struct sockaddr_in address;
    SSL_CTX *ssl_ctx;
    thread_pool_t thread_pool;
    server_config_t config;
} server_t;

int server_init(server_t *server, const char *config_file);
int server_start(server_t *server);
void server_cleanup(server_t *server);
void handle_client_task(void *arg);

#endif
