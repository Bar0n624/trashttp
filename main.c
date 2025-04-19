#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <regex.h>

#define PORT 8080
#define NUM_WORKERS 10
#define MAX_CLIENTS 45
#define BUFFER_SIZE 1024
#define DIRECTORY "/home/bar0n/Documents/hp/trashttp"

typedef struct server {
    int server_fd;
    struct sockaddr_in address;
} SERVER;

typedef struct {
    pthread_t threads[NUM_WORKERS];
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int *client_fds[MAX_CLIENTS];
    int count;
    int head;
    int tail;
} thread_pool_t;

thread_pool_t pool;

void *handle_client(void *arg);

void *worker_thread(void *arg) {
    while (1) {
        int *client_fd;

        pthread_mutex_lock(&pool.lock);
        while (pool.count == 0) {
            pthread_cond_wait(&pool.cond, &pool.lock);
        }

        client_fd = pool.client_fds[pool.head];
        pool.head = (pool.head + 1) % MAX_CLIENTS;
        pool.count--;

        pthread_mutex_unlock(&pool.lock);

        handle_client((void *)client_fd);
        free(client_fd);
    }
    return NULL;
}

void thread_pool_init(thread_pool_t *pool) {
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);
    pool->count = 0;
    pool->head = 0;
    pool->tail = 0;

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, NULL);
    }
}

void thread_pool_add_task(thread_pool_t *pool, int *client_fd) {
    pthread_mutex_lock(&pool->lock);

    pool->client_fds[pool->tail] = client_fd;
    pool->tail = (pool->tail + 1) % MAX_CLIENTS;
    pool->count++;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
}

void thread_pool_destroy(thread_pool_t *pool) {
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_cancel(pool->threads[i]);
        pthread_join(pool->threads[i], NULL);
    }
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;

    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    size_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_read > 0) {
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1.1", REG_EXTENDED);
        regmatch_t match[2];
        if (regexec(&regex, buffer, 2, match, 0) == 0) {
            char *filename = strndup(buffer + match[1].rm_so, match[1].rm_eo - match[1].rm_so);

            if (strcmp(filename, "") == 0 || filename == NULL) {
                filename = strdup("index.html");
            }

            printf("Requested file: %s\n", filename);

            FILE *file = fopen(filename, "r");
            if (file) {
                const char *response_header = "HTTP/1.1 200 OK\r\n"
                                              "Content-Type: text/html\r\n"
                                              "Connection: close\r\n\r\n";
                send(client_fd, response_header, strlen(response_header), 0);
                char file_buffer[BUFFER_SIZE];
                bytes_read = 0;
                while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > (size_t)0) {
                    send(client_fd, file_buffer, bytes_read, 0);
                }
                fclose(file);
            } else {
                const char *not_found_response = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, not_found_response, strlen(not_found_response), 0);
            }
            free(filename);
        } else {
            const char *bad_request_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(client_fd, bad_request_response, strlen(bad_request_response), 0);
        }
    }

    free(buffer);
    close(client_fd);
    return NULL;
}

int main(void) {
    chdir(DIRECTORY);

    SERVER server;
    server.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.server_fd <= 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(PORT);

    int res = bind(server.server_fd, (struct sockaddr *)&server.address, sizeof(server.address));

    if (res < 0) {
        perror("Could not bind socket to port");
        exit(EXIT_FAILURE);
    }

    res = listen(server.server_fd, 45);

    if (res < 0) {
        perror("Could not listen on socket");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    thread_pool_init(&pool);

    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        *client_fd = accept(server.server_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (*client_fd < 0) {
            perror("Could not accept connection");
            free(client_fd);
            continue;
        }

        thread_pool_add_task(&pool, client_fd);
    }

    close(server.server_fd);
    thread_pool_destroy(&pool);
    return 0;
}

//TODO: add some form of thread scheduling
//TODO: add many many more http features, possibly http2 with alpn as well
//TODO: add ssl support
//TODO: add a config file
//TODO: add a makefile
//TODO: add a proper logging system
//TODO: add a proper error handling system
//TODO: add support for not just get requests (lmfao)
//TODO: configure reverse proxy