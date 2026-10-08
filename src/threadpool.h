#ifndef THREADPOOL_H
#define THREADPOOL_H

#define TASK_PAYLOAD_SIZE 64
#include "config.h"

typedef struct Task {
    void (*function)(void *arg);
    u8 payload[TASK_PAYLOAD_SIZE];
} Task;

typedef struct ThreadPool {
    pthread_t *threads;
    Task *tasks;
    int thread_count;
    int queue_size;
    int head, tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t notify;
    int shutdown;
} ThreadPool;

typedef struct Worker {
  ThreadPool* pool;
  Config* conf;
} Worker;

extern _Thread_local Worker *current_worker;

#endif
