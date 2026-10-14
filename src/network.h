#ifndef NETWORK_H
#define NETWORK_H

// networking helper functions
#include <netinet/in.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RECV_CAP KB(64)

extern String8 CRLF;

typedef enum { SMTP, POP3 } Protocol;

String8 protocol_strings[] = {{.str = (u8*)"SMTP", .size = 4},
                              {.str = (u8*)"POP3", .size = 4}};

void* get_in_addr(struct sockaddr* sa);
b32 recv_all(int fd, String8* buf);
b32 recv_until_crlf(int fd, String8* buf);
b32 recv_until_dot_crlf(int fd, String8* buf);
b32 send_all(int fd, String8 data);
int connect_socket_get(String8 addr, String8 port);
int listen_socket_get(String8 addr, String8 port);

#endif
