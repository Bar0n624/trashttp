#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "http.h"

#define PORT 8080
#define NUM_WORKERS 10
#define MAX_CLIENTS 45
#define BUFFER_SIZE 1024

typedef struct server {
    int server_fd;
    struct sockaddr_in address;
} SERVER;

typedef struct work_item {
    void (*function)(void *arg);
    void *arg;
    struct work_item *next;
} work_item_t;

typedef struct {
    work_item_t *front;
    work_item_t *rear;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} work_queue_t;

typedef struct {
    pthread_t threads[NUM_WORKERS];
    work_queue_t queue;
} thread_pool_t;

thread_pool_t pool;

void work_queue_init(work_queue_t *queue) {
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void work_queue_push(work_queue_t *queue, void (*function)(void *), void *arg) {
    work_item_t *item = malloc(sizeof(work_item_t));
    item->function = function;
    item->arg = arg;
    item->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->rear) {
        queue->rear->next = item;
    } else {
        queue->front = item;
    }
    queue->rear = item;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

work_item_t *work_queue_pop(work_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (!queue->front) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    work_item_t *item = queue->front;
    queue->front = item->next;
    if (!queue->front) {
        queue->rear = NULL;
    }
    pthread_mutex_unlock(&queue->lock);
    return item;
}

void work_queue_destroy(work_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->front) {
        work_item_t *item = queue->front;
        queue->front = item->next;
        free(item);
    }
    pthread_mutex_unlock(&queue->lock);
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
}

void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    while (1) {
        work_item_t *item = work_queue_pop(&pool->queue);
        item->function(item->arg);
        free(item);
    }
    return NULL;
}

void thread_pool_init(thread_pool_t *pool) {
    work_queue_init(&pool->queue);
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
}

void thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    work_queue_push(&pool->queue, function, arg);
}

void thread_pool_destroy(thread_pool_t *pool) {
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_cancel(pool->threads[i]);
        pthread_join(pool->threads[i], NULL);
    }
    work_queue_destroy(&pool->queue);
}

void handle_client_task(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';

    http_request_t request;
    if (parse_http_request(buffer, &request) != 0) {
        const char *bad_request_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, bad_request_response, strlen(bad_request_response), 0);
        close(client_fd);
        return;
    }

    handle_http1_request(client_fd, NULL, &request);
}

int main(void) {
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

        thread_pool_add_task(&pool, handle_client_task, client_fd);
    }

    close(server.server_fd);
    thread_pool_destroy(&pool);
    return 0;
}


//TODO: add a config file
//TODO: add a makefile
//TODO: add a proper logging system
//TODO: add a proper error handling system
//TODO: add support for not just get requests (lmfao)
//TODO: configure reverse proxy