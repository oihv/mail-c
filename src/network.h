#ifndef NETWORK_H
#define NETWORK_H

// networking helper functions

// network includes
#include <netinet/in.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

// SSL includes
#pragma push_macro("fn") // temporarily undefine
#undef fn
#include <openssl/ssl.h>
#pragma pop_macro("fn")
#include <openssl/err.h>

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

typedef struct SSL_Result {
  SSL* ssl;
  b32 ok;
} Net_SSL;

#define SSL_ERR_LEN 256

// TLS
SSL_CTX *ssl_ctx = NULL;   // global, created once
fn void tls_init(void);
fn void tls_cleanup(void);
fn Net_SSL tls_connect(int fd);

// helper
fn void* get_in_addr(struct sockaddr* sa);

// socket
fn Net_Socket net_connect(String8 addr, String8 port);
fn Net_Socket net_listen(String8 addr, String8 port);

// operation
fn Net_Result net_recv(int fd, u8* buf, u64 cap);
fn Net_Result net_recv_tls(SSL* ssl, u8* buf, u64 cap);
fn Net_Result recv_until_crlf(int fd, String8* buf);
fn Net_Result recv_until_crlf_tls(SSL* ssl, String8* buf);
fn Net_Result recv_until_dot_crlf(int fd, String8* buf);
fn Net_Result recv_until_dot_crlf_tls(SSL* ssl, String8* buf);
fn Net_Result send_all(int fd, String8 data);
fn Net_Result send_all_tls(SSL* ssl, String8 data);


#endif
