/*
** client.c -- a stream socket client demo
*/

#include "base/base.h"
#include "base64.h"
#include "config.h"
#include "db.h"
#include "network.h"
#include "smtp.h"
#include "pop3.h"
#include "imf.h"
#include "cli.h"
#include "threadpool.h"

#include "base/base.c"
#include "base64.c"
#include "config.c"
#include "db.c"
#include "network.c"
#include "parse.c"
#include "smtp.c"
#include "pop3.c"
#include "imf.c"
#include "cli.c"

int main(int argc, char* argv[]) {
  Arena* perm_arena = arena_alloc(MB(64), KB(64));
  log_active = log_alloc();
  log_scope_begin();
  CLI ctx = {0};

  if (argc != 2) {
    fprintf(stderr, "Config file not provided\nusage: server <config file>\n");
    goto cleanup;
  }

  ctx.conf = config_from_file(perm_arena, str8_cstring(argv[1]));

  // setup socket
  Net_Socket conn_result = net_connect(ctx.conf.addr, ctx.conf.pop3_port);
  if (conn_result.status != Net_Status_Ok) {
    goto cleanup;
  }
  ctx.pop3_fd = conn_result.fd;

  // connect tls
  Net_SSL ssl_result = tls_connect(ctx.pop3_fd);
  if (ssl_result.ok == false) {
    goto cleanup;
  }
  ctx.pop3_ssl = ssl_result.ssl;

  // recv buffer
  String8 recv_buf = {.str = arena_push(perm_arena, RECV_CAP + 1, true),
                      .size = 0};
  ctx.recv_buf = recv_buf;

  // check for server ready message
  recv_until_crlf_tls(ctx.pop3_ssl, &ctx.recv_buf);
  if (log_scope_peek_count(LogMsgKind_UserError) > 0)
    goto cleanup;

  i32 res = pop3_is_response_ok(ctx.recv_buf);
  if (log_scope_peek_count(LogMsgKind_UserError) > 0)
    goto cleanup;

  if (res == 0) {
    printf("POP3 server is not ready\n");
    goto cleanup;
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
  tls_cleanup();

cleanup:;
  LogScopeResult log = log_scope_end(perm_arena, LogMsgKind_All);
  append_data_to_file_path(ctx.conf.log_path, log.string);

  arena_release(perm_arena);
  return 0;
}
