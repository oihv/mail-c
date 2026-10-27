fn b32 pop3_is_response_ok(String8 rep);
fn b32 pop3_send_and_response_ok(int fd, String8 cmd, String8* buf);
fn String8 pop3_process_line(Arena* arena, Session* state, String8 line);
