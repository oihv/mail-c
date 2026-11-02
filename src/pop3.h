#ifndef POP3_H
#define POP3_H

typedef enum POP3_Status {
  POP3_Ok,
  POP3_Err,
} POP3_Reply;

typedef struct POP3_Result {
  POP3_Reply reply;
  b32 ok;
} POP3_Result;

fn POP3_Result pop3_is_response_ok(String8 rep);
fn POP3_Result pop3_send_and_response_ok(int fd, String8 cmd, String8* buf);
fn POP3_Result pop3_send_and_response_ok_tls(SSL* ssl, String8 cmd, String8* buf);
fn String8 pop3_process_line(Arena* arena, Session* state, String8 line);

#endif // POP3_H
