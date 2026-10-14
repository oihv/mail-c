/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "base/base.h"
#include "base/base.c"
#include "db.c"
#include "network.h"
#include "network.c"
#include "parse.c"
__thread u32 log_thread_id = 0; // since some logs requires thread id
#include "smtp.c"
#include "pop3.c"
#include "imf.c"

typedef enum {
  CLI_State_Login,
  CLI_State_Menu,
  CLI_State_Inbox,
  CLI_State_Compose,
  CLI_State_Quit,
  CLI_State_COUNT
} CLI_State;

typedef struct {
  CLI_State current_state;
  String8 server_addr;
  String8 user_email;
  String8 smtp_port;
  String8 pop3_port;
  int pop3_fd;
  String8 recv_buf;
  String8 log_path;
} CLI;

// Large enough for the biggest single input line (the recipient list, which is
// MAX_RECIPIENT addresses of up to MAX_USERNAME_LENGTH bytes separated by ';').
#define CLI_INPUT_CAP 4096

// NOTE: AI
// Reads one line from stdin, consuming the trailing newline, trims surrounding
// whitespace, and copies the result into the arena. Because the newline is
// consumed here, callers never need to drain stdin before another read. Returns
// an empty string on EOF.
String8 cli_read_line(Arena* arena) {
  u8 buf[CLI_INPUT_CAP];
  if (!fgets((char*)buf, sizeof(buf), stdin)) {
    return (String8){0};
  }
  return str8_skip_chop_whitespace(str8_copy(arena, str8_cstring((char*)buf)));
}

#define MAX_USERNAME_LENGTH 256
#define MAX_PASS_LENGTH 256
void cli_login(Arena* arena, CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 username = {0};
  String8 password = {0};

  while (1) {
    Temp scratch2 = arena_scratch_get(&scratch.arena, 1);
    printf("Enter your email address:\n");
    username = cli_read_line(scratch.arena);

    log_scope_begin();
    b32 res =
        pop3_send_and_response_ok(ctx->pop3_fd,
                                  str8f(scratch2.arena, "USER %.*s%.*s",
                                        str8_fmt(username), str8_fmt(CRLF)),
                                  &ctx->recv_buf);
    LogScopeResult log = log_scope_end(scratch2.arena);
    append_data_to_file_path(ctx->log_path, log.strings[LogMsgKind_Info]);
    if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
      append_data_to_file_path(ctx->log_path,
                               log.strings[LogMsgKind_UserError]);
      continue;
    }

    if (res == 0) {
      printf("Username not recognized by the server.\nTry again\n");
      continue;
    }

    arena_scratch_release(scratch2);
    break;
  }

  while (1) {
    Temp scratch2 = arena_scratch_get(&scratch.arena, 1);
    printf("Enter the password for username %.*s:\n", str8_fmt(username));
    password = cli_read_line(scratch.arena);

    log_scope_begin();
    b32 res =
        pop3_send_and_response_ok(ctx->pop3_fd,
                                  str8f(scratch2.arena, "PASS %.*s%.*s",
                                        str8_fmt(password), str8_fmt(CRLF)),
                                  &ctx->recv_buf);
    LogScopeResult log = log_scope_end(scratch2.arena);
    append_data_to_file_path(ctx->log_path, log.strings[LogMsgKind_Info]);
    if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
      append_data_to_file_path(ctx->log_path,
                               log.strings[LogMsgKind_UserError]);
      continue;
    }

    if (res == 0) {
      printf("Wrong password!\nTry Again\n");
      continue;
    }

    arena_scratch_release(scratch2);
    break;
  }
  printf("Login successful\n");

  ctx->user_email = str8_copy(arena, username);
  ctx->current_state = CLI_State_Menu;

  arena_scratch_release(scratch);
}

void cli_menu(CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  printf("---EMAIL CLIENT MENU---\n[1] check inbox\n[2] compose and send "
         "email\n[q] quit\n");
  printf("Select menu (1,2,q):\n");

  b32 ok = 0;
  while (!ok) {
    String8 opt = cli_read_line(scratch.arena);
    ok = 1;

    if (str8_compare_cstring(opt, "1")) {
      ctx->current_state = CLI_State_Inbox;
    } else if (str8_compare_cstring(opt, "2")) {
      ctx->current_state = CLI_State_Compose;
    } else if (str8_compare_cstring(opt, "q")) {
      ctx->current_state = CLI_State_Quit;
      printf("Quitting...\n");
    } else {
      printf("Unexpected option, try again\n");
      ok = 0;
    }
  }
  arena_scratch_release(scratch);
}

void cli_inbox(CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  if (!send_all(ctx->pop3_fd,
                str8f(scratch.arena, "LIST%.*s", str8_fmt(CRLF)))) {
    printf("Error when trying to send LIST request\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  if (!recv_until_dot_crlf(ctx->pop3_fd, &ctx->recv_buf)) {
    printf("Error when trying to receive LIST response\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  if (!pop3_is_response_ok(ctx->recv_buf)) {
    printf("Server response error\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  // parse the result
  u64 total_mail = 0;
  u64 total_octet = 0;
  i64Array octets = {0};
  {
    ctx->recv_buf = str8_skip_chop_whitespace(ctx->recv_buf);
    String8List lines = str8_split(scratch.arena, ctx->recv_buf, CRLF);

    // parse the summary
    {
      // +OK x messages (y octets)
      String8 first_line = lines.first->string;
      String8List chunks = str8_split(scratch.arena, first_line, str8_lit(" "));
      total_mail = str8_to_u64(chunks.first->next->string);

      String8 octets = chunks.first->next->next->next->string;
      if (octets.str[0] == '(') {
        octets.str++;
        octets.size -= 1;
      } else {
        printf("Unexpected reply token\n");
        ctx->current_state = CLI_State_Menu;
        return;
      }
      total_octet = str8_to_u64(octets);
    }

    lines.first = lines.first->next; // advance by 1

    // parse the list
    {
      octets.v = arena_push(scratch.arena, total_mail * sizeof(u64), 1);
      for
        EachNode(line, String8Node, lines.first) {
          if (line->next != 0) { // ignore last line (.\r\n)
            String8List chunks =
                str8_split(scratch.arena, line->string, str8_lit(" "));
            octets.v[octets.count++] = str8_to_u64(chunks.first->next->string);
          }
        }
    }

    // print the summary
    {
      printf("--- INBOX FOR USER %.*s ---\n", str8_fmt(ctx->user_email));
      printf("Total mails: %lu\nTotal octets: %lu\n", total_mail, total_octet);
      printf("\tIndex\tOctets\t\n");
      for
        EachIndex(i, octets.count) {
          printf("\t%lu\t%lu\t\n", i + 1, octets.v[i]);
        }
    }
  }

  if (octets.count > 0) {
    printf("[1] - [%lu]: select mail\n", octets.count);
  }
  printf("[q]        : quit\n");

  // get input
  u64 choice = 0;
  while (1) {
    String8 input = cli_read_line(scratch.arena);

    if (str8_compare_cstring(input, "q")) {
      ctx->current_state = CLI_State_Menu;
      return;
    }

    choice = str8_to_u64(input);
    if (choice < 1 || choice > octets.count) {
      printf("Selection out of range, try again\n");
      continue;
    }
    break;
  }

  // request to server RETR
  if (!send_all(ctx->pop3_fd,
                str8f(scratch.arena, "RETR %lu%.*s", choice, str8_fmt(CRLF)))) {
    printf("Error when trying to send RETR request\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  if (!recv_until_dot_crlf(ctx->pop3_fd, &ctx->recv_buf)) {
    printf("Error when trying to receive RETR response\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  if (!pop3_is_response_ok(ctx->recv_buf)) {
    printf("Server response error\n");
    ctx->current_state = CLI_State_Menu;
    return;
  }

  // skip the OK message
  u64 pos = str8_find(ctx->recv_buf, CRLF);
  pos += CRLF.size;
  ctx->recv_buf.str += pos;
  ctx->recv_buf.size -= pos;
  printf("%.*s", str8_fmt(ctx->recv_buf));

  printf("[enter to continue.]");
  cli_read_line(scratch.arena);
  arena_scratch_release(scratch);
}

#define MAX_RECIPIENT 15
#define MAX_DATA_LINE 1064
void cli_compose(CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);

  printf("Recipient To: (separate with \";\" for multiple recipients)\n");
  String8 recipients = cli_read_line(scratch.arena);

  String8List recipient_list =
      str8_split(scratch.arena, recipients, str8_lit(";"));

  String8 data_line = {
      .str = arena_push(scratch.arena, sizeof(u8) * MAX_DATA_LINE, 1),
      .size = 0};
  String8List data_list = {0};
  printf("Enter your message: (end with blank line with \".\")\n");
  while (1) {
    if (fgets((char*)data_line.str, sizeof(u8) * MAX_DATA_LINE, stdin)) {
      data_line.size = strlen((char*)data_line.str);
      data_line = str8_skip_chop_whitespace(data_line);
      if (!str8_compare_cstring(data_line, ".")) {
        String8 data_copy = str8_copy(scratch.arena, data_line);
        str8_list_push(scratch.arena, &data_list, data_copy);
      } else {
        break;
      }
    }
  }

  StringJoin join = {.sep = CRLF, .post = CRLF};
  String8 data_joined = str8_list_join(scratch.arena, &data_list, &join);

  String8 data =
      imf_message_compose(scratch.arena, imf_local_date(scratch.arena),
                          ctx->user_email, data_joined);

  log_scope_begin();

  b32 ret = 0;
  i32 smtp_fd = connect_socket_get(ctx->server_addr, ctx->smtp_port);
  ret = recv_until_crlf(smtp_fd, &ctx->recv_buf);
  LogScopeResult log = log_scope_end(scratch.arena);
  append_data_to_file_path(ctx->log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(ctx->log_path, log.strings[LogMsgKind_UserError]);
    printf("[error] occured, read log for more info\n");
    return;
  }

  if (ret == 0) {
    printf("recv failed, read log for more info\n");
    return;
  }

  if (smtp_get_reply_code(ctx->recv_buf) != RC_SERVICE_READY) {
    printf("SMTP server is not ready: reply %.*s\n", str8_fmt(ctx->recv_buf));
    return;
  };

  String8List message_list = {0};
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_ehlo(scratch.arena, ctx->server_addr));
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_from(scratch.arena, ctx->user_email));
  for
    EachNode(recipient, String8Node, recipient_list.first) {
      str8_list_push(scratch.arena, &message_list,
                     smtp_command_to(scratch.arena, recipient->string));
    }
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_data(scratch.arena));
  str8_list_push(scratch.arena, &message_list, data);
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_quit(scratch.arena));

  b32 message_sent = 1;
  b32 is_sending_data = 0;
  for
    EachNode(message, String8Node, message_list.first) {
      if (!send_all(smtp_fd, message->string)) {
        printf("send error when trying to send %.*s\n",
               str8_fmt(message->string));
        message_sent = 0;
        break;
      }

      if (is_sending_data) {
        if (str8_compare_cstring(message->string, ".\r\n")) {
          is_sending_data = 0;
        } else {
          continue;
        }
      }

      if (!recv_until_crlf(smtp_fd, &ctx->recv_buf)) {
        printf("recv failed\n");
        message_sent = 0;
        break;
      }

      SMTP_ReplyCode ok = RC_OK;
      if (str8_compare_cstring(str8_substr(message->string, r1u64(0, 4)),
                               "DATA")) {
        ok = RC_START_MAIL_INPUT;
        is_sending_data = 1;
      }

      if (smtp_get_reply_code(ctx->recv_buf) != ok) {
        printf("SMTP bad reply, message: %.*s\nreply: %.*s\n",
               str8_fmt(message->string), str8_fmt(ctx->recv_buf));
        message_sent = 0;
        break;
      };
    }

  if (message_sent) {
    printf("mail successfully sent!\n");
  } else {
    printf("failed trying to send mail\n");
  }
  ctx->current_state = CLI_State_Menu;
  arena_scratch_release(scratch);
}

int main(int argc, char* argv[]) {
  Arena* perm_arena = arena_alloc(MB(64), KB(64));
  log_active = log_alloc();
  CLI ctx = {0};

  if (argc != 4) {
    fprintf(stderr, "usage: client [hostname] [SMTP port] [POP3 port] <client "
                    "log path>\n");
    exit(1);
  }
  ctx.log_path = argc == 5 ? str8_cstring(argv[4]) : str8_lit("client.log");
  ctx.server_addr = str8_cstring(argv[1]);

  // setup socket
  ctx.smtp_port = str8_cstring(argv[2]);
  ctx.pop3_port = str8_cstring(argv[3]);
  ctx.pop3_fd = connect_socket_get(ctx.server_addr, ctx.pop3_port);

  LogScopeResult log = log_scope_end(perm_arena);
  append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_UserError]);
    exit(1);
  }

  String8 recv_buf = {.str = arena_push(perm_arena, RECV_CAP + 1, true),
                      .size = 0};
  ctx.recv_buf = recv_buf;

  log_scope_begin();
  recv_until_crlf(ctx.pop3_fd, &ctx.recv_buf);
  log = log_scope_end(perm_arena);
  append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_UserError]);
    exit(1);
  }

  log_scope_begin();
  i32 res = pop3_is_response_ok(ctx.recv_buf);
  log = log_scope_end(perm_arena);
  append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_Info]);
  if (log_active && log.strings[LogMsgKind_UserError].size != 0) {
    append_data_to_file_path(ctx.log_path, log.strings[LogMsgKind_UserError]);
    exit(1);
  }

  if (res == 0) {
    printf("POP3 server is not ready\n");
    exit(1);
  }

  printf("Connected to server\n");

  ctx.current_state = CLI_State_Login;

  while (ctx.current_state != CLI_State_Quit) {
    switch (ctx.current_state) {
    case CLI_State_Login:
      cli_login(perm_arena, &ctx);
      break;
    case CLI_State_Menu:
      cli_menu(&ctx);
      break;
    case CLI_State_Inbox:
      cli_inbox(&ctx);
      break;
    case CLI_State_Compose:
      cli_compose(&ctx);
      break;
    case CLI_State_Quit:
      break;
    case CLI_State_COUNT:
      break;
    }
  }

  close(ctx.pop3_fd);

  return 0;
}
