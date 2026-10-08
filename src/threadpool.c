#include <pthread.h>
#include "threadpool.h"

_Thread_local Worker *current_worker = NULL;

static void* thread_worker(void *arg) {
    Worker *w = (Worker*)arg;
    current_worker = w;
    ThreadPool* pool = w->pool;
    
    while (1) {
        pthread_mutex_lock(&pool->lock);
        
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        
        // Get task
        Task task = pool->tasks[pool->head];
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count--;
        
        pthread_mutex_unlock(&pool->lock);
        
        // Execute task
        task.function(task.payload);
    }
    pthread_exit(NULL);
}

ThreadPool* threadpool_create(Arena* arena, Config* conf, int thread_count, int queue_size) {
    Worker* worker = arena_push(arena, sizeof(Worker), 1);
    ThreadPool *pool = arena_push(arena, sizeof(ThreadPool), 1);
    pool->threads = arena_push(arena, sizeof(pthread_t) * thread_count, 1);
    pool->tasks = arena_push(arena, sizeof(Task) * queue_size, 1);
    
    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = 0;

    worker->conf = conf;
    worker->pool = pool;
    
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->notify, NULL);
    
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&pool->threads[i], NULL, thread_worker, worker);
    }
    
    return pool;
}

int threadpool_add(ThreadPool *pool, void (*function)(void *), void* payload, u64 payload_size) {
    pthread_mutex_lock(&pool->lock);
    
    if (pool->count == pool->queue_size) {
        pthread_mutex_unlock(&pool->lock);
        return -1; // Queue full
    }
    
    pool->tasks[pool->tail].function = function;
    memcpy(pool->tasks[pool->tail].payload, payload, payload_size);
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count++;
    
    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
    
    return 0;
}

int threadpool_destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
    
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    return 0;
}

