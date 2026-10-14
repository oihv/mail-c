#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sqlite3.h>

#define TASK_PAYLOAD_SIZE KB(4)
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
  u32 id;
  String8 log_path;
  Arena* session_arena;
  sqlite3* db;
} Worker;

extern __thread Worker *current_worker;
extern __thread u32 log_thread_id;

#endif
