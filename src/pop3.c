#include "base/base.h"
#include "threadpool.h"
extern String8 CRLF;

b32 pop3_is_response_ok(String8 rep) {
  if (str8_compare_cstring(str8_substr(rep, r1u64(0, 3)), "+OK")) {
    log_infof("[info] server responsed with OK, response:\n%.*s\n",
              str8_fmt(rep));
    return 1;
  } else if (str8_compare_cstring(str8_substr(rep, r1u64(0, 4)), "-ERR")) {
    log_infof("[info] server responsed with ERR, response:\n%.*s\n",
              str8_fmt(rep));
    return 0;
  } else {
    log_user_errorf("[error] unrecognized POP3 server response\nResponse:%.*s\n", str8_fmt(rep));
    return -1;
  }
}

b32 pop3_send_and_response_ok(int fd, String8 cmd, String8* buf) {
  if (!send_all(fd, cmd)) {
    log_user_error(
        str8_lit("[error] POP3 command hasn't been completely sent\n"));
    return 1;
  }
  if (!recv_until_crlf(fd, buf)) {
    log_user_error(str8_lit("[error] when receiving POP3 response\n"));
    return 0;
  }
  return pop3_is_response_ok(*buf);
}

String8 pop3_process_line(Arena* arena, Session* state, String8 line) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 reply;

  if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "USER")) {
    String8List list = str8_split(scratch.arena, line, str8_lit(" "));
    if (list.node_count != 2) {
      log_user_errorf("[t%u] [error] USER contains more than one username\n",
                      log_thread_id);
      reply =
          str8f(arena, "-ERR USER command contains more than one username%.*s",
                str8_fmt(CRLF));
    } else {
      String8 username = list.last->string;
      log_infof("[t%u] [info] Successfully parsed username: %.*s\n",
                log_thread_id, str8_fmt(username));
      // check to db first
      // dummy checker
      if (str8_compare_cstring(username, "coba@test.com")) {
        reply = str8f(arena, "+OK USER %.*s found in database%.*s",
                      str8_fmt(username), str8_fmt(CRLF));
        state->sender = str8_copy(arena, username);
      } else {
        reply = str8f(arena, "-ERR user not found%.*s", str8_fmt(CRLF));
      }
    }
  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "PASS")) {
    String8List list = str8_split(scratch.arena, line, str8_lit(" "));
    if (list.node_count != 2) {
      log_user_errorf(
          "[t%u] [error] PASS command contains more than one password\n",
          log_thread_id);
      reply =
          str8f(arena, "-ERR PASS command contains more than one password%.*s",
                str8_fmt(CRLF));
    } else {
      String8 sender = list.last->string;
      log_infof("[t%u] [info] Successfully parsed password\n", log_thread_id,
                str8_fmt(state->username));
      // check to db first
      // dummy checker
      if (str8_compare_cstring(sender, "1234")) {

        reply = str8f(arena, "+OK PASS for user %.*s is correct%.*s",
                      str8_fmt(state->username), str8_fmt(CRLF));
      } else {
        reply = str8f(arena, "-ERR password incorrect%.*s", str8_fmt(CRLF));
      }
    }
  }
  return reply;
}
