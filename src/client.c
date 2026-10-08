/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include "base/base.h"
#include "base/base.c"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 3) {
    fprintf(stderr, "usage: client hostname port\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
              sizeof s);
    printf("client: attempting connection to %s\n", s);

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      perror("client: connect");
      close(sockfd);
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof s);
  printf("client: connected to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  Arena* arena = arena_alloc(MB(100), KB(1));
  String8 buff_str = {.size = 512};
  ssize_t n = recv(sockfd, buff_str.str, buff_str.size, 0);
  // if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
  //     perror("recv");
  //     exit(1);
  // }
  //
  // buf[numbytes] = '\0';
  //
  // printf("client: received '%s'\n",buf);
  String8 send_buf = str8_lit("EHLO wocao\r\nnihao tongxue\r\nanjay\r\n");
  send(sockfd, send_buf.str, send_buf.size, 0);

  close(sockfd);

  return 0;
}
