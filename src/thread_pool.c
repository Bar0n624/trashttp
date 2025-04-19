#include "thread_pool.h"
#include <stdlib.h>

static void *worker_thread(void *arg);

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

static void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    while (pool->is_running) {
        work_item_t *item = work_queue_pop(&pool->queue);
        item->function(item->arg);
        free(item);
    }
    return NULL;
}

int thread_pool_init(thread_pool_t *pool, int num_threads) {
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) return -1;

    pool->num_threads = num_threads;
    pool->is_running = 1;
    work_queue_init(&pool->queue);

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            thread_pool_destroy(pool);
            return -1;
        }
    }
    return 0;
}

void thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    work_queue_push(&pool->queue, function, arg);
}

void thread_pool_destroy(thread_pool_t *pool) {
    pool->is_running = 0;

    // Push empty tasks to wake up all threads
    for (int i = 0; i < pool->num_threads; i++) {
        work_queue_push(&pool->queue, NULL, NULL);
    }

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    work_queue_destroy(&pool->queue);
    free(pool->threads);
}