/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include "base/base.h"
#include "base/base.c"
#include "threadpool.h"
#include "threadpool.c"
#include "network.c"
#include "parse.c"
#include "smtp.c"
#include "pop3.c"

typedef enum {
  CLI_State_Login,
  CLI_State_Menu,
  CLI_State_Inbox,
  CLI_State_Compose,
  CLI_State_COUNT
} CLI_State;

typedef struct {
  CLI_State current_state;
  String8 user_email;
  String8 smtp_port;
  String8 pop3_port;
  int smtp_fd;
  int pop3_fd;
  String8 recv_buf;
  String8 log_path;
} CLI;

#define MAX_USERNAME_LENGTH 256
#define MAX_PASS_LENGTH 256
void cli_login(Arena* arena, CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 username = {
      .str = arena_push(scratch.arena, sizeof(u8) * MAX_USERNAME_LENGTH, 1),
      .size = 0};
  String8 password = {
      .str = arena_push(scratch.arena, sizeof(u8) * MAX_PASS_LENGTH, 1),
      .size = 0};

  while (1) {
    Temp scratch2 = arena_scratch_get(&scratch.arena, 1);
    printf("Enter your email address:\n");
    scanf("%s", username.str);
    username.size = strlen((char*)username.str);
    username.str[username.size] = 0;
    username = str8_skip_chop_whitespace(username);

    log_scope_begin();
    b32 res =
        pop3_send_and_response_ok(ctx->pop3_fd,
                                  str8f(scratch2.arena, "USER %.*s %.*s",
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
    printf("Enter the password for username %.*s:\n", str8_fmt(username));
    scanf("%s", password.str);
    password.size = strlen((char*)password.str);
    password.str[password.size] = 0;

    // TODO: match to db
    if (!str8_compare_cstring(password, "1234")) {
      printf("Wrong password!\nTry Again\n");
      continue;
    }

    break;
  }
  printf("Login successful\n");

  ctx->user_email = str8_copy(arena, username);

  arena_scratch_release(scratch);
}

void cli_menu(CLI* ctx) {
  printf("---EMAIL CLIENT MENU---\n[1] check inbox\n[2] compose and send "
         "email\n[q] quit\n");
  char opt;
  b32 ok = 0;
  while (!ok) {
    printf("Select menu (1,2,q):\n");

    scanf("%s", &opt);

    switch (opt) {
    case '1':
      ctx->current_state = CLI_State_Inbox;
      ok = 1;
      break;
    case '2':
      ctx->current_state = CLI_State_Compose;
      ok = 1;
      break;
    case 'q':
      printf("Quitting...\n");
      exit(1);
    default:
      printf("Unexpected option, try again\n");
      break;
    }
  }
}

void cli_inbox(CLI* ctx) {
  printf("TODO inbox\n");
  scanf("");
}

void cli_compose(CLI* ctx) {
  printf("TODO compose\n");
  scanf("");
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

  // setup socket
  ctx.smtp_port = str8_cstring(argv[2]);
  ctx.pop3_port = str8_cstring(argv[3]);
  ctx.pop3_fd =
      connect_socket_get(str8_cstring(argv[1]), ctx.pop3_port);

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
  cli_login(perm_arena, &ctx);

  ctx.current_state = CLI_State_Menu;

  while (1) {
    switch (ctx.current_state) {
    case CLI_State_Login:
    case CLI_State_Menu:
      cli_menu(&ctx);
      break;
    case CLI_State_Inbox:
      cli_inbox(&ctx);
      break;
    case CLI_State_Compose:
      cli_compose(&ctx);
      break;
    case CLI_State_COUNT:
      break;
    }
  }

  // String8 send_buf = str8_lit("EHLO wocao\r\nMAIL FROM: anjay\r\nRCPT TO:
  // woi\r\nDATA\r\nQUIT\r\n"); String8 send_buf = str8_lit("MAIL FROM:
  // <nikolas@bit.com>   \r\n");
  String8 send_buf = str8_lit(
      "EHLO\r\nMAIL FROM: <nikolas@bit.com>\r\nRCPT TO: "
      "<supreme@witch.calamitas>\r\nDATA\r\nHalo bro\r\nDone\r\n.\r\nQUIT\r\n");
  send(ctx.pop3_fd, send_buf.str, send_buf.size, 0);
  while (1)
    ;

  close(ctx.pop3_fd);

  return 0;
}
