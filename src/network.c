#include "network.h"

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// not really a receive "all" it's just a wrapper to check for errors
// conveniently
b32 recv_all(int fd, String8* buf) {
  ssize_t n = recv(fd, buf->str, RECV_CAP, 0);
  if (n == -1) {
    log_user_errorf("[error] at recv %s\n", strerror(errno));
    return 0;
  } else if (n == 0) {
    log_infof("[info] target closed the connection\n");
    return 0;
  } else {
    buf->str[n] = 0;
    buf->size = n;
    log_infof("[info] recv %u bytes\n recv_buff: %.*s\n", n, str8_fmt(*buf));
    return 1;
  }
}

b32 recv_until_crlf(int fd, String8* buf) {
  if (!recv_all(fd, buf)) {
    return 0;
  }
  while (!str8_compare(str8_substr(*buf, r1u64(buf->size - 2, buf->size)),
                      CRLF)) {
    log_infof("[info] calling recv_all again since crlf is not found\n");
    if (!recv_all(fd, buf)) {
      return 0;
    }
  }
  return 1;
}

b32 recv_until_dot_crlf(int fd, String8* buf) {
  if (!recv_all(fd, buf)) {
    return 0;
  }
  while (!str8_compare_cstring(str8_substr(*buf, r1u64(buf->size - 3, buf->size)),
                      ".\r\n")) {
    log_infof("[info] calling recv_all again since .\r\n is not found\n");
    if (!recv_all(fd, buf)) {
      return 0;
    }
  }
  return 1;
}

b32 send_all(int fd, String8 data) {
  u64 sent = 0;
  while (sent < data.size) {
    // MSG_NOSIGNAL to ignore SIGPIPE from stopping the process
    ssize_t n = send(fd, data.str + sent, data.size - sent, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return 0;
    }
    if (n == 0)
      return 0;
    sent += (u64)n;
  }
  return 1;
}

int connect_socket_get(String8 addr, String8 port) {
  log_scope_begin();
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int status;
  char s[INET6_ADDRSTRLEN];
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo((char*)addr.str, (char*)port.str, &hints,
                            &servinfo)) != 0) {
    log_user_errorf("[error] at getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }

  // loop through all the results and connect to the first we can
  int yes = 1;
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      log_infof("[error] at socket call: %s\n", strerror(errno));
      continue;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s,
              sizeof s);
    log_infof("[info] attempting connection to %s with port %.*s\n", s, str8_fmt(port));

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      log_user_errorf("[error] at setsockopt: %s\n", strerror(errno));
      break;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      log_infof("[error] connect: %s\n", strerror(errno));
      close(sockfd);
      continue;
    }

    break;
  }

  if (p == NULL) {
    log_user_errorf("[error] failed to connect\n");
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s,
            sizeof s);
  log_infof("[info] connected to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  return sockfd;
}

int listen_socket_get(String8 addr, String8 port) {
  Temp scratch = arena_scratch_get(0, 0);
  struct addrinfo hints, *res, *p;
  int status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo((char*)addr.str, (char*)port.str, &hints, &res)) !=
      0) {
    log_user_errorf("[error] at getaddrinfo: %s\n", gai_strerror(status));
  }

  log_infof("[info] IP addresses for host %s:\n", addr);

  int sockfd;
  int yes = 1;
  char* addrstr = arena_push(scratch.arena, 52, 1);
  for (p = res; p != NULL; p = p->ai_next) {
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), addrstr, 52);
    log_infof("%s\n", addrstr);
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      log_infof("[error] at socket call: %s\n", strerror(errno));
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      log_user_errorf("[error] at setsockopt: %s\n", strerror(errno));
      break;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      log_infof("[info] fail at bind: %s\n", strerror(errno));
      continue;
    }

    break;
  }
  freeaddrinfo(res); // free the linked list

  if (p == NULL) {
    log_user_error(str8_lit("[error] failed to bind\n"));
  }

  if (listen(sockfd, 10) == -1) {
    log_user_errorf("[error] at listen: %s\n", strerror(errno));
  }

  return sockfd;
}
