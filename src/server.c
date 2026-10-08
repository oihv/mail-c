#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/base.h"
#include "base/base.c"
#include "config.h"
#include "config.c"
#include "smtp.c"
#include "threadpool.h"
#include "threadpool.c"

typedef struct {
  int fd;
  char ip[INET6_ADDRSTRLEN];
} ClientInfo;

#define RECV_CAP KB(4)

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void handle_client(void *payload) {
  ClientInfo *info = (ClientInfo *)payload;
  Arena *arena = arena_scratch_get(0, 0).arena;
  Config *conf = current_worker->conf;
  int new_fd = info->fd;
  String8 ready_rep = smtp_reply_ready(arena, conf->addr);
  if (send(new_fd, ready_rep.str, ready_rep.size, 0) == -1) {
    perror("send");
  }

  String8 recv_buff = {.str = arena_push(arena, RECV_CAP, true), .size = 0};
  String8 line_buff = {.str = arena_push(arena, RECV_CAP, true), .size = 0};

  while (1) {
    Temp scratch = arena_scratch_get(0, 0);
    // drain the line buffer
    String8 total = str8_concat(scratch.arena, line_buff, recv_buff);
    u64 pos = str8_find(total, CRLF);
    while (total.size >= 0 && pos != total.size) {
      String8 line = {.str = total.str, .size = pos};
      // process line
      // smtp_process_command(line);

      total.str = total.str + pos + CRLF.size;
      total.size -= pos;

      pos = str8_find(total, CRLF);
    }

    // store the remaining messages in the line buffer
    memcpy(line_buff.str, total.str, total.size);
    line_buff.size = total.size;

    // recv more message
    ssize_t n = recv(new_fd, recv_buff.str, RECV_CAP, 0);
    if (n == -1) {
      perror("recv");
      break;
    }
    if (n == 0) {
      // todo: log message server closed
      break;
    }
    recv_buff.str[n] = 0;
    recv_buff.size = n;
    printf("recv: %.*s\n", (int)n, recv_buff.str);

    arena_scratch_release(scratch);
  }

  close(new_fd);
}

int main(int argc, char *argv[]) {
  Arena *arena = arena_alloc(MB(100), KB(1));
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "usage: server <config file>\n");
    return 1;
  }

  Config conf = config_from_file(arena, str8_cstring(argv[1]));

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE; // use my ip

  if ((status = getaddrinfo((char *)conf.addr.str, (char *)conf.smtp_port.str,
                            &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  printf("IP addresses for %s:\n\n", argv[1]);

  int sockfd, new_fd;
  int yes = 1;
  char *addrstr = arena_push(arena, 52, 1);
  for (p = res; p != NULL; p = p->ai_next) {
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), addrstr, 52);
    printf("  %s\n", addrstr);
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(res); // free the linked list

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, 10) == -1) {
    perror("listen");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  // TODO: take arg for configuration of num of threads
  ThreadPool *pool = threadpool_create(arena, &conf, 8, 32);

  struct sockaddr_storage their_addr; // connector's address info
  socklen_t sin_size;
  char *addr_buff = arena_push(arena, INET6_ADDRSTRLEN, true);
  while (1) { // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              addr_buff, INET6_ADDRSTRLEN);
    printf("server: got connection from %s\n", addr_buff);

    ClientInfo info = {.fd = new_fd};
    memcpy(info.ip, addr_buff, INET6_ADDRSTRLEN);

    threadpool_add(pool, handle_client, &info, sizeof(info));
  }

  return 0;
}
