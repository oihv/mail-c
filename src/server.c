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
#include "config.c"
#include "smtp.c"

void sigchld_handler(int s) {
  (void)s; // quiet unused variable warning

  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
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

  if ((status = getaddrinfo((char *)conf.addr.str, (char *)conf.smtp_port.str, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  printf("IP addresses for %s:\n\n", argv[1]);

  int sockfd, new_fd;
  int yes = 1;
  char* addrstr = arena_push(arena, 52, 1);
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

  // reap all dead process
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  struct sockaddr_storage their_addr; // connector's address info
  socklen_t sin_size;
  char *addr_buff = arena_push(arena, INET6_ADDRSTRLEN, true);
  String8 recv_buff = {.size = 512};
  recv_buff.str = arena_push(arena, recv_buff.size + 1, true);
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

    if (!fork()) {   // this is the child process
      close(sockfd); // child doesn't need the listener
      // if (send(new_fd, "Hello, world!", 13, 0) == -1)
      //   perror("send");
      recv(new_fd, recv_buff.str, recv_buff.size, 0);
      printf("recv: %s\n", recv_buff.str);
      close(new_fd);
      exit(0);
    }
    close(new_fd); // parent doesn't need this
  }
  return 0;
}
