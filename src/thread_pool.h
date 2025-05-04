#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

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
    int task_count;
} work_queue_t;

typedef struct {
    pthread_t *threads;
    int num_threads;
    work_queue_t queue;
    work_queue_t *local_queues;
    pthread_mutex_t *queue_locks;
    int is_running;
    int enable_work_stealing;
} thread_pool_t;

typedef struct {
    thread_pool_t *pool;
    int thread_id;
} worker_args_t;

void work_queue_init(work_queue_t *queue);
void work_queue_push(work_queue_t *queue, void (*function)(void *), void *arg);
work_item_t *work_queue_pop(work_queue_t *queue);
work_item_t *work_queue_try_pop(work_queue_t *queue);
void work_queue_destroy(work_queue_t *queue);

int thread_pool_init(thread_pool_t *pool, int num_threads, int enable_work_stealing);
void thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg);
void thread_pool_destroy(thread_pool_t *pool);

#endif
