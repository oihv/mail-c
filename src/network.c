// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// not really a receive "all" it's just a wrapper to check for errors
// conveniently
Net_Result net_recv(int fd, u8* buf, u64 cap) {
  Net_Result out = {0};
  ssize_t n = recv(fd, buf, cap, 0);
  if (n == -1) {
    log_user_errorf("at recv %s\n", strerror(errno));
    out.status = Net_Status_RecvFailed;
    out.sys_error = errno;
  } else if (n == 0) {
    log_infof("target closed the connection\n");
    out.status = Net_Status_Eof;
  } else {
    buf[n] = 0;
    log_infof("recv %u bytes\n recv_buff: %s\n", n, buf);
    out.status = Net_Status_Ok;
    out.bytes = n;
  }
  return out;
}

Net_Result recv_until_crlf(int fd, String8* buf) {
  u64 bytes_sent = 0;
  Net_Result result = net_recv(fd, buf->str, RECV_CAP);
  Net_Result out = {0};
  out.status = Net_Status_Ok;
  if (result.status != Net_Status_Ok) {
    out.status = result.status;
    return out;
  }
  buf->size = result.bytes;
  bytes_sent += result.bytes;
  // TODO: need to adjust RECV_CAP after each call
  while (
      !str8_compare(str8_substr(*buf, r1u64(buf->size - 2, buf->size)), CRLF)) {
    log_infof("calling recv_all again since crlf is not found\n");
    result = net_recv(fd, buf->str + bytes_sent, RECV_CAP - bytes_sent);
    if (result.status != Net_Status_Ok) {
      out.status = result.status;
      return out;
    }
    buf->size += result.bytes;
    bytes_sent += result.bytes;
  }
  return out;
}

Net_Result recv_until_dot_crlf(int fd, String8* buf) {
  u64 bytes_sent = 0;
  Net_Result result = net_recv(fd, buf->str, RECV_CAP);
  Net_Result out = {0};
  out.status = Net_Status_Ok;
  if (result.status != Net_Status_Ok) {
    out.status = result.status;
    return out;
  }
  buf->size = result.bytes;
  bytes_sent += result.bytes;
  // TODO: need to adjust RECV_CAP after each call
  while (
      !str8_compare_cstring(str8_substr(*buf, r1u64(buf->size - 3, buf->size)), ".\r\n")) {
    log_infof("calling recv_all again since crlf is not found\n");
    result = net_recv(fd, buf->str + bytes_sent, RECV_CAP - bytes_sent);
    if (result.status != Net_Status_Ok) {
      out.status = result.status;
      return out;
    }
    buf->size += result.bytes;
    bytes_sent += result.bytes;
  }
  return out;
}

Net_Result send_all(int fd, String8 data) {
  log_infof("sending data:\n%.*s", str8_fmt(data));
  Net_Result out = {0};
  out.status = Net_Status_Ok;
  while (out.bytes < data.size) {
    // MSG_NOSIGNAL to ignore SIGPIPE from stopping the process
    ssize_t n =
        send(fd, data.str + out.bytes, data.size - out.bytes, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      out.status = Net_Status_SendFailed;
      out.sys_error = errno;
      break;
    }
    if (n == 0) {
      out.status = Net_Status_Eof;
      break;
    }
    out.bytes += (u64)n;
  }
  if (out.bytes == data.size) log_info(str8_lit("successfully sent data"));
  else log_user_error(str8_lit("data has not been completely sent\n"));
  return out;
}

Net_Socket net_connect(String8 addr, String8 port) {
  Net_Socket out = {0};
  struct addrinfo hints, *servinfo, *p;
  int status;
  char s[INET6_ADDRSTRLEN];
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo((char*)addr.str, (char*)port.str, &hints,
                            &servinfo)) != 0) {
    log_user_errorf("at getaddrinfo: %s\n", gai_strerror(status));
    out.status = Net_Status_ConnectFailed;
    return out;
  }

  // loop through all the results and connect to the first we can
  int yes = 1;
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((out.fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      log_infof("at socket call: %s\n", strerror(errno));
      continue;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s,
              sizeof s);
    log_infof("attempting connection to %s with port %.*s\n", s,
              str8_fmt(port));

    if (setsockopt(out.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      log_user_errorf("at setsockopt: %s\n", strerror(errno));
      out.status = Net_Status_ConnectFailed;
      break;
    }

    if (connect(out.fd, p->ai_addr, p->ai_addrlen) == -1) {
      log_infof("at connect: %s\n", strerror(errno));
      close(out.fd);
      continue;
    }

    break;
  }
  freeaddrinfo(servinfo);

  if (p == NULL) {
    log_user_errorf("failed to connect\n");
    out.status = Net_Status_ConnectFailed;
    return out;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s,
            sizeof s);
  log_infof("connected to %s\n", s);

  out.status = Net_Status_Ok;
  return out;
}

Net_Socket net_listen(String8 addr, String8 port) {
  Net_Socket out;
  Temp scratch = arena_scratch_get(0, 0);
  struct addrinfo hints, *res, *p;
  int status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo((char*)addr.str, (char*)port.str, &hints, &res)) !=
      0) {
    log_user_errorf("at getaddrinfo: %s\n", gai_strerror(status));
    out.status = Net_Status_ListenFailed;
    return out;
  }

  log_infof("IP addresses for host %s:\n", addr);

  int yes = 1;
  char* addrstr = arena_push(scratch.arena, 52, 1);
  for (p = res; p != NULL; p = p->ai_next) {
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), addrstr, 52);
    log_infof("%s\n", addrstr);
    if ((out.fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      log_infof("at socket call: %s\n", strerror(errno));
      continue;
    }

    if (setsockopt(out.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      log_user_errorf("at setsockopt: %s\n", strerror(errno));
      out.status = Net_Status_ListenFailed;
      break;
    }

    if (bind(out.fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(out.fd);
      log_infof("fail at bind: %s\n", strerror(errno));
      continue;
    }

    break;
  }
  freeaddrinfo(res); // free the linked list

  if (p == NULL) {
    log_user_error(str8_lit("failed to bind\n"));
    out.status = Net_Status_ListenFailed;
    return out;
  }

  if (listen(out.fd, 10) == -1) {
    log_user_errorf("at listen: %s\n", strerror(errno));
    out.status = Net_Status_ListenFailed;
    return out;
  }

  out.status = Net_Status_Ok;
  return out;
}
