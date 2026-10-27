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

typedef enum Net_Status {
  Net_Status_Ok = 1,
  Net_Status_Eof, // orderly close, not an error, connection closed
  Net_Status_Interrupted,
  Net_Status_WouldBlock,
  Net_Status_ResolveFailed, // getaddrinfo / EAI_*
  Net_Status_SocketFailed,
  Net_Status_ConnectFailed,
  Net_Status_BindFailed,
  Net_Status_ListenFailed,
  Net_Status_SendFailed,
  Net_Status_RecvFailed,
  Net_Status_ProtocolError, // malformed/oversized input
  Net_Status_OutOfMemory,
} Net_Status;

typedef struct Net_Result {
  Net_Status status;
  u64 bytes;     // transferred / consumed by this operation
  i32 sys_error; // errno or EAI_* captured immediately
} Net_Result;

typedef struct Net_Socket {
  Net_Status status;
  i32 fd;
} Net_Socket;

Net_Socket net_connect(String8 addr, String8 port);
Net_Socket net_listen(String8 addr, String8 port);

void* get_in_addr(struct sockaddr* sa);
Net_Result net_recv(int fd, u8* buf, u64 cap);
Net_Result recv_until_crlf(int fd, String8* buf);
Net_Result recv_until_dot_crlf(int fd, String8* buf);
Net_Result send_all(int fd, String8 data);

#endif
