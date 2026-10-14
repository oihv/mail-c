#include <pthread.h>
#include "threadpool.h"
#include "base/base.h"
#include "db.h"

__thread Worker* current_worker = NULL;
__thread u32 log_thread_id = 0;

static void* thread_worker(void* arg) {
  Worker* w = (Worker*)arg;
  current_worker = w;
  log_active = log_alloc();
  log_thread_id = w->id;
  ThreadPool* pool = w->pool;
  w->session_arena = arena_alloc(MB(64), KB(64));

  while (1) {
    Temp scratch = arena_scratch_get(0, 0);
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
    log_scope_begin();
    log_infof("[t%u] [info] got task\n", log_thread_id);
    LogScopeResult log = log_scope_end(scratch.arena);
    append_data_to_file_path(current_worker->log_path, log.strings[LogMsgKind_Info]);
    arena_scratch_release(scratch);

    pthread_mutex_unlock(&pool->lock);

    // Execute task
    task.function(task.payload);
  }
  pthread_exit(NULL);
}

ThreadPool* threadpool_create(Arena* arena, Config* conf, int thread_count,
                              int queue_size) {
  Worker* workers = arena_push(arena, sizeof(Worker) * thread_count, 1);
  ThreadPool* pool = arena_push(arena, sizeof(ThreadPool), 1);
  pool->threads = arena_push(arena, sizeof(pthread_t) * thread_count, 1);
  pool->tasks = arena_push(arena, sizeof(Task) * queue_size, 1);

  pool->thread_count = thread_count;
  pool->queue_size = queue_size;
  pool->head = pool->tail = pool->count = 0;
  pool->shutdown = 0;

  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->notify, NULL);

  for (int i = 0; i < thread_count; i++) {
    workers[i].conf = conf;
    workers[i].pool = pool;
    workers[i].id = i;
    workers[i].db = db_open(conf->db_path);
    workers[i].log_path =
        str8f(arena, "%.*s.t%u", str8_fmt(conf->server_log_path), i);

    pthread_create(&pool->threads[i], NULL, thread_worker, &workers[i]);
  }

  return pool;
}

int threadpool_add(ThreadPool* pool, void (*function)(void*), void* payload,
                   u64 payload_size) {
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

int threadpool_destroy(ThreadPool* pool) {
  pthread_mutex_lock(&pool->lock);
  pool->shutdown = 1;
  pthread_cond_broadcast(&pool->notify);
  pthread_mutex_unlock(&pool->lock);

  for (int i = 0; i < pool->thread_count; i++) {
    pthread_join(pool->threads[i], NULL);
  }

  return 0;
}
