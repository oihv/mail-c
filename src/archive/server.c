#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#include "base/base.h"
#include "base/base.c"
#include "db.c"
#include "threadpool.h"
#include "threadpool.c"
#include "config.h"
#include "config.c"
#include "network.h"
#include "network.c"
#include "smtp.c"
#include "pop3.c"
#include "parse.c"

static b32 shutdown_flag = 0;

typedef struct {
  int fd;
  char ip[INET6_ADDRSTRLEN];
  Protocol proto;
  ThreadPool* pool;
  String8 log_path;
  sqlite3* db;
} ClientInfo;

void handle_client(void* payload) {
  ClientInfo* info = (ClientInfo*)payload;
  Arena* arena = arena_scratch_get(0, 0).arena;
  Config* conf = current_worker->conf;
  String8 log_path = current_worker->log_path;
  int new_fd = info->fd;
  bool quit = false;
  Session state = {0};
  state.conf = conf;
  state.arena = current_worker->session_arena;
  state.temp = temp_begin(state.arena);
  state.db = current_worker->db;

  log_scope_begin();
  log_infof("[t%u] [info] handling connection from %s, with protocol %.*s\n",
            log_thread_id, info->ip, str8_fmt(protocol_strings[info->proto]));
  String8 ready_rep = info->proto == SMTP ? smtp_reply_ready(arena, conf->addr) : str8f(arena, "+OK %.*s POP3 server ready%.*s", str8_fmt(conf->addr), str8_fmt(CRLF));
  if (!send_all(new_fd, ready_rep)) {
    log_user_errorf("[t%u] [error] at send: %s\n", log_thread_id,
                    strerror(errno));
    quit = 1;
  }
  log_infof("[t%u] [info] sent ready reply\n", log_thread_id);
  LogScopeResult log = log_scope_end(arena);
  append_data_to_file_path(log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(log_path, log.strings[LogMsgKind_UserError]);
    quit = 1;
  }

  String8 recv_buff = {.str = arena_push(arena, RECV_CAP, true), .size = 0};
  String8 line_buff = {.str = arena_push(arena, RECV_CAP, true), .size = 0};

  while (!quit) {
    Temp scratch = arena_scratch_get(&arena, 1);
    log_scope_begin();

    // drain the line buffer
    String8 total = str8_concat(scratch.arena, line_buff, recv_buff);
    u64 pos = str8_find(total, CRLF);
    while (total.size >= 0 && pos != total.size) {
      String8 line = {.str = total.str, .size = pos};
      log_infof("[t%u] [info] processing line:\n%.*s\n", log_thread_id,
                str8_fmt(line));
      String8 reply = info->proto == SMTP ? smtp_process_line(scratch.arena, &state, line) : pop3_process_line(scratch.arena, &state, line);

      if (reply.size > 0) {
        if (!send_all(new_fd, reply)) {
          log_user_errorf("[t%u] [error] when trying to reply at send: %s\n",
                          log_thread_id, strerror(errno));
          quit = 1;
          break;
        }

        if (info->proto == SMTP && smtp_get_reply_code(reply) == RC_CLOSING_TRANSMISSION_CHANNEL ||
            info->proto == POP3 && str8_compare_cstring(line, "QUIT")) {
          log_infof("[t%u] [info] client requested QUIT command\n", log_thread_id);
          quit = true;
          break;
        }
        log_infof("[t%u] [info] sent reply:\n    %.*s", log_thread_id,
                  str8_fmt(reply));
      }

      total.str = total.str + pos + CRLF.size;
      total.size -= pos + CRLF.size;

      pos = str8_find(total, CRLF);
    }

    // store the remaining messages in the line buffer
    memcpy(line_buff.str, total.str, total.size);
    line_buff.size = total.size;

    // recv more message
    if (!quit) {
      ssize_t n = recv(new_fd, recv_buff.str, RECV_CAP, 0);
      if (n == -1) {
        log_user_errorf("[t%u] [error] at recv %s\n", log_thread_id,
                        strerror(errno));
        quit = 1;
      } else if (n == 0) {
        log_infof("[t%u] [info] client closed the connection\n", log_thread_id);
        quit = 1;
      } else {
        recv_buff.str[n] = 0;
        recv_buff.size = n;
        log_infof("[t%u] [info] recv %u bytes\n recv_buff: %.*s", log_thread_id,
                  n, str8_fmt(recv_buff));
      }
    }

    LogScopeResult log = log_scope_end(scratch.arena);
    append_data_to_file_path(current_worker->log_path,
                             log.strings[LogMsgKind_Info]);
    if (log_active != 0 && log.strings[LogMsgKind_UserError].size > 0) {
      append_data_to_file_path(current_worker->log_path,
                               log.strings[LogMsgKind_UserError]);
      quit = 1;
    }
    arena_scratch_release(scratch);
  }

  temp_end(state.temp);
  close(new_fd);
}

void* acceptor_thread(void* arg) {
  Temp scratch1 = arena_scratch_get(0, 0);
  ClientInfo* info = (ClientInfo*)arg;
  int fd = info->fd;
  log_active = log_alloc();
  while (!shutdown_flag) {
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    int new_fd;
    char* addr_buff = arena_push(scratch1.arena, INET6_ADDRSTRLEN, true);
    while (1) { // main accept() loop
      Temp scratch = arena_scratch_get(&scratch1.arena, 1);
      log_scope_begin();
      sin_size = sizeof their_addr;
      new_fd = accept(fd, (struct sockaddr*)&their_addr, &sin_size);
      if (new_fd == -1) {
        log_user_errorf("[%.*s] [error] at accept: %s\n",
                        str8_fmt(protocol_strings[info->proto]),
                        strerror(errno));
        continue;
      }

      inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr*)&their_addr), addr_buff,
                INET6_ADDRSTRLEN);
      log_infof("[%.*s] [info] got connection from %s\n",
                str8_fmt(protocol_strings[info->proto]), addr_buff);
      LogScopeResult log = log_scope_end(scratch.arena);
      append_data_to_file_path(info->log_path, log.strings[LogMsgKind_Info]);
      if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
        append_data_to_file_path(info->log_path,
                                 log.strings[LogMsgKind_UserError]);
        exit(1);
      }

      ClientInfo new_info = {.fd = new_fd, .proto = info->proto};
      memcpy(new_info.ip, addr_buff, INET6_ADDRSTRLEN);

      threadpool_add(info->pool, handle_client, &new_info, sizeof(new_info));
      arena_scratch_release(scratch);
    }
    arena_scratch_release(scratch1);
  }

  pthread_exit(0);
}

int main(int argc, char* argv[]) {
  Arena* arena = arena_alloc(MB(100), KB(1));
  char ipstr[INET6_ADDRSTRLEN];
  log_active = log_alloc();

  if (argc != 2) {
    fprintf(stderr, "Config file not provided\nusage: server <config file>\n");
    return 1;
  }

  // read config
  log_scope_begin();
  log_info(str8_lit("[info] starting server...\n"));
  Config conf = config_from_file(arena, str8_cstring(argv[1]));
  LogScopeResult log = log_scope_end(arena);
  append_data_to_file_path(conf.server_log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(conf.server_log_path,
                             log.strings[LogMsgKind_UserError]);
    exit(1);
  }

  // setup socket
  log_scope_begin();
  log_info(str8_lit("[info] getting listening socket...\n"));
  int smtp_fd = listen_socket_get(conf.addr, conf.smtp_port);
  int pop3_fd = listen_socket_get(conf.addr, conf.pop3_port);
  log = log_scope_end(arena);
  append_data_to_file_path(conf.server_log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(conf.server_log_path,
                             log.strings[LogMsgKind_UserError]);
    exit(1);
  }

  log_scope_begin();
  log_info(str8_lit("[info] got listening sockets...\n"));

  // TODO: take arg for configuration of num of threads
  ThreadPool* pool = threadpool_create(arena, &conf, 8, 32);

  ClientInfo arg1 = {
      .fd = smtp_fd,
      .proto = SMTP,
      .pool = pool,
      .log_path = str8f(arena, "%.*s.smtp", str8_fmt(conf.server_log_path))};
  ClientInfo arg2 = {
      .fd = pop3_fd,
      .proto = POP3,
      .pool = pool,
      .log_path = str8f(arena, "%.*s.pop3", str8_fmt(conf.server_log_path))};

  pthread_t t_smtp, t_pop3;
  pthread_create(&t_smtp, NULL, acceptor_thread, &arg1);
  pthread_create(&t_pop3, NULL, acceptor_thread, &arg2);

  log_info(str8_lit("[info] acceptor_thread for SMTP and POP3 port started\n"));
  log_info(str8_lit("[info] threadpool created\n"));
  log_info(str8_lit("[info] waiting for connections...\n"));
  log = log_scope_end(arena);
  append_data_to_file_path(conf.server_log_path, log.strings[LogMsgKind_Info]);

  printf("press 'q' to shutdown the server\n");
  while (getchar() != 'q')
    ;
  printf("shutting down server...\n");
  shutdown_flag = 1;
  shutdown(smtp_fd, SHUT_RDWR);
  shutdown(pop3_fd, SHUT_RDWR);
  pthread_join(t_smtp, NULL);
  pthread_join(t_pop3, NULL);
  threadpool_destroy(pool);

  return 0;
}
