#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "server.h"


static void *worker_thread(void *arg);
static void *worker_thread_stealing(void *arg);
static work_item_t *try_steal_work(const thread_pool_t *pool, int self_id);

void work_queue_init(work_queue_t *queue) {
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    queue->task_count = 0;
}

scheduler_type_t get_scheduler_type(const char *scheduler_str) {
    if (!scheduler_str) {
        return SCHEDULER_ROUND_ROBIN;
    }

    if (strcmp(scheduler_str, "LEAST_CONNECTIONS") == 0) {
        return SCHEDULER_LEAST_CONNECTIONS;
    } else if (strcmp(scheduler_str, "RANDOM") == 0) {
        return SCHEDULER_RANDOM;
    } else {
        return SCHEDULER_ROUND_ROBIN;
    }
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
    queue->task_count++;

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
    queue->task_count--;
    pthread_mutex_unlock(&queue->lock);
    return item;
}

work_item_t *work_queue_try_pop(work_queue_t *queue) {
    if (pthread_mutex_trylock(&queue->lock) != 0) {
        return NULL;
    }

    work_item_t *item = NULL;
    if (queue->front) {
        item = queue->front;
        queue->front = item->next;
        if (!queue->front) {
            queue->rear = NULL;
        }
        queue->task_count--;
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
    worker_args_t *worker_args = (worker_args_t *)arg;
    thread_pool_t *pool = worker_args->pool;
    int thread_id = worker_args->thread_id;

    while (pool->is_running) {
        work_item_t *item = work_queue_pop(&pool->queue);
        if (item->function && item->arg) {
            client_args_t *client_args = (client_args_t *)item->arg;
            client_args->thread_id = thread_id;
        }
        item->function(item->arg);
        free(item);
    }
    return NULL;
}

void thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    if (!pool->is_running) return;

    if (!pool->enable_work_stealing) {
        work_queue_push(&pool->queue, function, arg);
        return;
    }

    int queue_id = 0;
    static int next_queue = 0;

    switch (pool->scheduler) {
        case SCHEDULER_ROUND_ROBIN: {
            queue_id = next_queue;
            next_queue = (next_queue + 1) % pool->num_threads;
            break;
        }

        case SCHEDULER_LEAST_CONNECTIONS: {
            int min_tasks = INT_MAX;

            for (int i = 0; i < pool->num_threads; i++) {
                pthread_mutex_lock(&pool->local_queues[i].lock);
                int tasks = pool->local_queues[i].task_count;
                pthread_mutex_unlock(&pool->local_queues[i].lock);

                if (tasks < min_tasks) {
                    min_tasks = tasks;
                    queue_id = i;
                }
            }
            break;
        }

        case SCHEDULER_RANDOM: {
            queue_id = rand() % pool->num_threads;
            break;
        }

        default:
            queue_id = next_queue;
        next_queue = (next_queue + 1) % pool->num_threads;
        break;
    }

    work_queue_push(&pool->local_queues[queue_id], function, arg);
}

int thread_pool_init(thread_pool_t *pool, const int num_threads, const int enable_work_stealing, scheduler_type_t scheduler) {
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    srand(time(NULL));
    if (!pool->threads) return -1;

    pool->num_threads = num_threads;
    pool->is_running = 1;
    pool->enable_work_stealing = enable_work_stealing;
    pool->scheduler = scheduler;
    work_queue_init(&pool->queue);

    if (enable_work_stealing) {
        pool->local_queues = malloc(sizeof(work_queue_t) * num_threads);

        if (!pool->local_queues) {
            free(pool->threads);
            return -1;
        }

        for (int i = 0; i < num_threads; i++) {
            work_queue_init(&pool->local_queues[i]);
        }

        for (int i = 0; i < num_threads; i++) {
            worker_args_t *arg = malloc(sizeof(worker_args_t));
            if (!arg) {
                thread_pool_destroy(pool);
                return -1;
            }
            arg->pool = pool;
            arg->thread_id = i + 1;

            if (pthread_create(&pool->threads[i], NULL, worker_thread_stealing, arg) != 0) {
                free(arg);
                thread_pool_destroy(pool);
                return -1;
            }
        }
    } else {
        pool->local_queues = NULL;

        for (int i = 0; i < num_threads; i++) {
            worker_args_t *arg = malloc(sizeof(worker_args_t));
            if (!arg) {
                thread_pool_destroy(pool);
                return -1;
            }
            arg->pool = pool;
            arg->thread_id = i + 1;
            if (pthread_create(&pool->threads[i], NULL, worker_thread, arg) != 0) {
                free(arg);
                thread_pool_destroy(pool);
                return -1;
            }
        }
    }

    return 0;
}

static void *worker_thread_stealing(void *arg) {
    const worker_args_t *worker_args = (worker_args_t *)arg;
    thread_pool_t *pool = worker_args->pool;
    const int thread_id = worker_args->thread_id;
    const int queue_index = thread_id - 1;

    while (pool->is_running) {
        work_item_t *item = NULL;
        item = work_queue_try_pop(&pool->local_queues[queue_index]);

        if (!item) {
            item = try_steal_work(pool, queue_index);

            if (!item) {
                usleep(1000);
                continue;
            }
        }
        if (!pool->is_running && !item->function) {
            free(item);
            break;
        }

        if (item->function && item->arg) {
            client_args_t *client_args = (client_args_t *)item->arg;
            client_args->thread_id = thread_id;
        }

        item->function(item->arg);
        free(item);
    }

    return NULL;
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool->is_running) return;

    pool->is_running = 0;
    if (pool->enable_work_stealing) {
        for (int i = 0; i < pool->num_threads; i++) {
            work_queue_push(&pool->local_queues[i], NULL, NULL);
        }

    } else {
        for (int i = 0; i < pool->num_threads; i++) {
            work_queue_push(&pool->queue, NULL, NULL);
        }

    }
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    if (pool->enable_work_stealing) {
        for (int i = 0; i < pool->num_threads; i++) {
            work_queue_destroy(&pool->local_queues[i]);
        }
        free(pool->local_queues);
    }

    work_queue_destroy(&pool->queue);
    free(pool->threads);
}

static work_item_t *try_steal_work(const thread_pool_t *pool, const int self_id) {
    for (int i = 0; i < pool->num_threads; i++) {
        if (i == self_id) continue;

        work_item_t *item = work_queue_try_pop(&pool->local_queues[i]);
        if (item) {
            return item;
        }
    }

    return NULL;
}

