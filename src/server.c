#include "server.h"
#include "ssl_utils.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "logger.h"
#include <stdatomic.h>

atomic_int next_thread_id = 1;

typedef struct {
    int client_fd;
    SSL_CTX *ssl_ctx;
    int buffer_size;
} client_args_t;

char* base_dir;

int server_init(server_t *server, const char *config_file) {
    if (load_config(config_file, &server->config) != 0) {
        fprintf(stderr, "Failed to load config from %s\n", config_file);
        return -1;
    }
    base_dir = server->config.base_dir;
    server->ssl_ctx = NULL;
    if (check_ssl_files(server->config.ssl_cert, server->config.ssl_key)) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        server->ssl_ctx = init_ssl_context(server->config.ssl_cert, server->config.ssl_key);
        if (!server->ssl_ctx) {
            fprintf(stderr, "Failed to initialize SSL context\n");
            return -1;
        }
        printf("SSL enabled with certificate and key\n");
    } else {
        printf("SSL files not found, running without SSL\n");
    }

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd <= 0) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server->server_fd);
        return -1;
    }

    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(server->config.port);

    if (bind(server->server_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0) {
        perror("Could not bind socket to port");
        close(server->server_fd);
        return -1;
    }

    if (thread_pool_init(&server->thread_pool, server->config.num_workers) != 0) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        close(server->server_fd);
        return -1;
    }

    return 0;
}

void handle_client_task(void *arg) {
    client_args_t *client_args = (client_args_t *)arg;
    int client_fd = client_args->client_fd;
    SSL_CTX *ssl_ctx = client_args->ssl_ctx;
    int buffer_size = client_args->buffer_size;
    free(client_args);

    int thread_id = atomic_fetch_add(&next_thread_id, 1);

    SSL *ssl = NULL;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        close(client_fd);
        return;
    }

    ssize_t bytes_read;

    //log request timestamp
    time_t request_time = time(NULL);
    log_message("Thread %d: Received request at %s", thread_id, ctime(&request_time));



    if (ssl_ctx) {
        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            log_message(" Thread %d: Error creating SSL structure", thread_id);
            free(buffer);
            close(client_fd);
            return;
        }

        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            log_message("Thread %d: SSL connection failed", thread_id);
            SSL_free(ssl);
            free(buffer);
            close(client_fd);
            return;
        }

        bytes_read = SSL_read(ssl, buffer, buffer_size - 1);
    } else {
        bytes_read = recv(client_fd, buffer, buffer_size - 1, 0);
    }

    if (bytes_read <= 0) {
        log_message("Thread %d: Failed to read from client", thread_id);
        if (ssl) SSL_free(ssl);
        free(buffer);
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';
    log_message("Thread %d: Received request from client: %s", thread_id, buffer);

    http_request_t request;
    if (parse_http_request(buffer, &request) != 0) {
        const char *bad_request_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        if (ssl) {
            SSL_write(ssl, bad_request_response, strlen(bad_request_response));
            SSL_free(ssl);
        } else {
            send(client_fd, bad_request_response, strlen(bad_request_response), 0);
        }

        //log response timestamp
        time_t response_time = time(NULL);
        log_message("Thread %d: Sent response at %s", thread_id, ctime(&response_time));

        free(buffer);
        close(client_fd);
        return;
    }

    free(buffer);
    handle_http1_request(client_fd, ssl, &request, base_dir);
}

int server_start(server_t *server) {
    if (listen(server->server_fd, server->config.max_clients) < 0) {
        perror("Could not listen on socket");
        return -1;
    }

    printf("Server listening on port %d\n", server->config.port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server->server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("Could not accept connection");
            continue;
        }

        client_args_t *client_args = malloc(sizeof(client_args_t));
        if (!client_args) {
            close(client_fd);
            continue;
        }

        client_args->client_fd = client_fd;
        client_args->ssl_ctx = server->ssl_ctx;
        client_args->buffer_size = server->config.buffer_size;

        thread_pool_add_task(&server->thread_pool, handle_client_task, client_args);
    }

    return 0;
}

void server_cleanup(server_t *server) {
    close(server->server_fd);
    thread_pool_destroy(&server->thread_pool);

    if (server->ssl_ctx) {
        SSL_CTX_free(server->ssl_ctx);
    }
}
