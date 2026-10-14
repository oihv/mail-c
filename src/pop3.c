#include "base/base.h"
#include "db.h"
#include "network.h"
extern String8 CRLF;
extern __thread u32 log_thread_id;

b32 pop3_is_response_ok(String8 rep) {
  if (str8_compare_cstring(str8_substr(rep, r1u64(0, 3)), "+OK")) {
    log_infof("[info] server responsed with OK%\n");
    return 1;
  } else if (str8_compare_cstring(str8_substr(rep, r1u64(0, 4)), "-ERR")) {
    log_infof("[info] server responsed with ERR\n");
    return 0;
  } else {
    log_user_errorf("[error] unrecognized POP3 server response\n");
    return -1;
  }
}

b32 pop3_send_and_response_ok(int fd, String8 cmd, String8* buf) {
  if (!send_all(fd, cmd)) {
    log_user_error(
        str8_lit("[error] POP3 command hasn't been completely sent\n"));
    return 0;
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
    log_infof("[t%u] [info] processing USER command\n", log_thread_id);
    String8List list = str8_split(scratch.arena, line, str8_lit(" "));
    if (list.node_count != 2) {
      log_user_errorf("[t%u] [error] USER contains more than one username\n",
                      log_thread_id);
      reply =
          str8f(arena, "-ERR USER command contains more than one username%.*s",
                str8_fmt(CRLF));
    } else {
      String8 username = list.last->string;
      username = str8_skip_chop_whitespace(username);
      log_infof("[t%u] [info] Successfully parsed username: %.*s\n",
                log_thread_id, str8_fmt(username));
      if (db_check_username_registered(state->db, username)) {
        reply = str8f(arena, "+OK USER %.*s found in database%.*s",
                      str8_fmt(username), str8_fmt(CRLF));
        log_infof("[t%u] [info] USER %.*s found in database\n", log_thread_id,
                  str8_fmt(username), str8_fmt(username));
        state->username = str8_copy(state->arena, username);
      } else {
        reply = str8f(arena, "-ERR user not found%.*s", str8_fmt(CRLF));
      }
    }

  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "PASS")) {
    log_infof("[t%u] [info] processing PASS command\n", log_thread_id);
    String8List list = str8_split(scratch.arena, line, str8_lit(" "));
    if (list.node_count != 2) {
      log_user_errorf(
          "[t%u] [error] PASS command contains more than one password\n",
          log_thread_id);
      reply =
          str8f(arena, "-ERR PASS command contains more than one password%.*s",
                str8_fmt(CRLF));
    } else {
      String8 password = list.last->string;
      password = str8_skip_chop_whitespace(password);
      log_infof("[t%u] [info] Successfully parsed password\n", log_thread_id);
      if (str8_compare(password, db_get_password_for_username(
                                     arena, state->db, state->username))) {
        reply = str8f(arena, "+OK PASS for user %.*s is correct%.*s",
                      str8_fmt(state->username), str8_fmt(CRLF));
      } else {
        reply = str8f(arena, "-ERR password incorrect%.*s", str8_fmt(CRLF));
      }
    }

  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "STAT")) {
    log_infof("[t%u] [info] processing STAT command\n", log_thread_id);
    u64 total_octets = db_get_mails_total_octets(state->db, state->username);
    u64 total_mail = db_get_mails_count(state->db, state->username);
    reply = str8f(scratch.arena, "+OK %d %d%.*s", total_mail, total_octets,
                  str8_fmt(CRLF));

  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "LIST")) {
    log_infof("[t%u] [info] processing LIST command\n", log_thread_id);
    String8List reply_list = {0};
    u64 total_octets = db_get_mails_total_octets(state->db, state->username);
    u64 total_mail = db_get_mails_count(state->db, state->username);
    str8_list_push(scratch.arena, &reply_list,
                   str8f(scratch.arena, "+OK %d messages (%d octets)",
                         total_mail, total_octets));
    i64Array octets =
        db_get_array_mail_octets(state->arena, state->db, state->username);

    for
      EachIndex(i, octets.count) {
        str8_list_push(scratch.arena, &reply_list,
                       str8f(scratch.arena, "%d %d", i + 1, octets.v[i]));
      }

    StringJoin join = {.sep = str8_lit("\r\n"), .post = str8_lit("\r\n.\r\n")};
    reply = str8_list_join(arena, &reply_list, &join);

    state->mail_ids = db_get_array_mail_ids(state->arena, state->db, state->username);
    state->octets = octets;

  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "RETR")) {
    log_infof("[t%u] [info] processing RETR command\n", log_thread_id);
    String8List list = str8_split(scratch.arena, line, str8_lit(" "));
    if (list.node_count != 2) {
      log_user_errorf(
          "[t%u] [error] RETR command contains more than one number\n",
          log_thread_id);
      reply =
          str8f(arena, "-ERR RETR command contains more than one number%.*s",
                str8_fmt(CRLF));
    } else {
      u64 index = str8_to_u64(list.first->next->string);
      u64 mail_id = state->mail_ids.v[index - 1];
      u64 octet = state->octets.v[index - 1];
      String8 mail = db_get_mail(arena, state->db, mail_id);
      StrEvalPrint(mail);
      reply = str8f(arena, "+OK %lu octets%.*s%.*s.%.*s", octet, str8_fmt(CRLF),
                    str8_fmt(mail), str8_fmt(CRLF));
    }

  } else if (str8_compare_cstring(str8_substr(line, r1u64(0, 4)), "QUIT")) {
    reply = str8f(arena, "+OK POP3 server signing off%.*s", str8_fmt(CRLF));
  }
  return reply;
}
