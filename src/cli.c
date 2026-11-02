// NOTE: AI
// Reads one line from stdin, consuming the trailing newline, trims surrounding
// whitespace, and copies the result into the arena. Because the newline is
// consumed here, callers never need to drain stdin before another read. Returns
// an empty string on EOF.
fn String8 cli_read_line(Arena* arena) {
  u8 buf[CLI_INPUT_CAP];
  if (!fgets((char*)buf, sizeof(buf), stdin)) {
    return (String8){0};
  }
  return str8_skip_chop_whitespace(str8_copy(arena, str8_cstring((char*)buf)));
}

#define MAX_USERNAME_LENGTH 256
#define MAX_PASS_LENGTH 256
fn void cli_login(Arena* arena, CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 username = {0};
  String8 password = {0};

  // get username
  while (1) {
    Temp scratch2 = arena_scratch_get(&scratch.arena, 1);
    printf("Enter your email address:\n");
    username = cli_read_line(scratch.arena);

    log_scope_begin();
    b32 res =
        pop3_send_and_response_ok_tls(ctx->pop3_ssl,
                                      str8f(scratch2.arena, "USER %.*s%.*s",
                                            str8_fmt(username), str8_fmt(CRLF)),
                                      &ctx->recv_buf);
    String8 error = log_scope_peek(scratch2.arena, LogMsgKind_UserError);
    if (error.size > 0) {
      append_data_to_file_path(ctx->conf.log_path, error);
      continue;
    }

    if (res == 0) {
      printf("Username not recognized by the server.\nTry again\n");
      continue;
    }

    arena_scratch_release(scratch2);
    break;
  }

  // get password
  b32 ok = 0;
  while (!ok) {
    Temp scratch2 = arena_scratch_get(&scratch.arena, 1);
    printf("Enter the password for username %.*s:\n", str8_fmt(username));
    password = cli_read_line(scratch.arena);

    log_scope_begin();
    b32 res =
        pop3_send_and_response_ok_tls(ctx->pop3_ssl,
                                      str8f(scratch2.arena, "PASS %.*s%.*s",
                                            str8_fmt(password), str8_fmt(CRLF)),
                                      &ctx->recv_buf);
    String8 error = log_scope_peek(scratch2.arena, LogMsgKind_UserError);
    if (error.size > 0) {
      append_data_to_file_path(ctx->conf.log_path, error);
      goto cleanup;
    }

    if (res == 0) {
      printf("Wrong password!\nTry Again\n");
      goto cleanup;
    }

    ok = 1;

  cleanup:;
    arena_scratch_release(scratch2);
  }
  printf("Login successful\n");

  ctx->username = str8f(arena, "%.*s@%.*s", str8_fmt(username),
                        str8_fmt(ctx->conf.mail_domain));
  ctx->password = str8_copy(arena, password);
  ctx->current_state = CLI_State_Menu;

  arena_scratch_release(scratch);
}

fn void cli_menu(CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);
  printf("---EMAIL CLIENT MENU---\nwelcome %.*s\n[1] check inbox\n[2] compose "
         "and send "
         "email\n[q] quit\n",
         str8_fmt(ctx->username));
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
  if (send_all_tls(ctx->pop3_ssl,
                   str8f(scratch.arena, "LIST%.*s", str8_fmt(CRLF)))
          .status != Net_Status_Ok) {
    printf("Error when trying to send LIST request\n");
    goto cleanup;
  }

  if (recv_until_dot_crlf_tls(ctx->pop3_ssl, &ctx->recv_buf).status !=
      Net_Status_Ok) {
    printf("Error when trying to receive LIST response\n");
    goto cleanup;
  }

  if (pop3_is_response_ok(ctx->recv_buf).ok == false) {
    printf("Server response error\n");
    goto cleanup;
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

      String8Node* octet_node = chunks.first->next->next->next;
      String8 octets = {0};
      if (octet_node) {
        octets = octet_node->string;
        if (octets.str[0] == '(') {
          octets.str++;
          octets.size -= 1;
        } else {
          printf("Unexpected reply token\n");
          goto cleanup;
        }
        total_octet = str8_to_u64(octets);
      }
    }

    lines.first = lines.first->next; // advance by 1

    // parse the list
    {
      octets.v = arena_push(scratch.arena, total_mail * sizeof(u64), 1);
      ForEachNode(line, String8Node, lines.first) {
        if (line->next != 0) { // ignore last line (.\r\n)
          String8List chunks =
              str8_split(scratch.arena, line->string, str8_lit(" "));
          octets.v[octets.count++] = str8_to_u64(chunks.first->next->string);
        }
      }
    }

    // print the summary
    {
      printf("--- INBOX FOR USER %.*s ---\n", str8_fmt(ctx->username));
      printf("Total mails: %lu\nTotal octets: %lu\n", total_mail, total_octet);
      if (octets.count > 0) {
        printf("\tIndex\tOctets\t\n");
      }
      ForEachIndex(i, octets.count) {
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
      goto cleanup;
    }

    choice = str8_to_u64(input);
    if (choice < 1 || choice > octets.count) {
      printf("Selection out of range, try again\n");
      continue;
    }
    break;
  }

  // request to server RETR
  if (send_all_tls(ctx->pop3_ssl,
                   str8f(scratch.arena, "RETR %lu%.*s", choice, str8_fmt(CRLF)))
          .status != Net_Status_Ok) {
    printf("Error when trying to send RETR request\n");
    goto cleanup;
  }

  if (recv_until_dot_crlf_tls(ctx->pop3_ssl, &ctx->recv_buf).status !=
      Net_Status_Ok) {
    printf("Error when trying to receive RETR response\n");
    goto cleanup;
  }

  if (pop3_is_response_ok(ctx->recv_buf).ok == false) {
    printf("Server response error\n");
    goto cleanup;
  }

  // skip the OK message
  u64 pos = str8_find(ctx->recv_buf, CRLF);
  pos += CRLF.size;
  ctx->recv_buf.str += pos;
  ctx->recv_buf.size -= pos;

  // omit the <crlf>.<crlf>
  ctx->recv_buf.size -= 3;

  // print the message
  printf("%.*s", str8_fmt(ctx->recv_buf));

  printf("[enter to continue.]");
  cli_read_line(scratch.arena);

cleanup:
  ctx->current_state = CLI_State_Menu;
  arena_scratch_release(scratch);
}

#define MAX_RECIPIENT 15
#define MAX_DATA_LINE 1064
void cli_compose(CLI* ctx) {
  Temp scratch = arena_scratch_get(0, 0);

  printf("Recipient To: (separate with ',' for multiple recipients)\n");
  String8 recipients = cli_read_line(scratch.arena);

  String8List recipient_list =
      str8_split(scratch.arena, recipients, str8_lit(","));

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
                          ctx->username, recipients, data_joined);

  log_scope_begin();

  Net_Result result = {0};

  Net_Socket conn_result = net_connect(ctx->conf.addr, ctx->conf.smtp_port);
  if (conn_result.status != Net_Status_Ok) {
    printf("Error when trying to connect to SMTP port\n");
    goto cleanup;
  }

  Net_SSL ssl_result = tls_connect(conn_result.fd);
  if (ssl_result.ok == false) {
    printf("Error when initiating TLS handshake to SMTP port\n");
    goto cleanup;
  }
  SSL* smtp_ssl = ssl_result.ssl;

  result = recv_until_crlf_tls(smtp_ssl, &ctx->recv_buf);
  if (result.status != Net_Status_Ok) {
    printf("Error when trying to recv SMTP response\n");
    goto cleanup;
  }

  if (smtp_get_reply_code(ctx->recv_buf) != RC_SERVICE_READY) {
    printf("SMTP server is not ready: reply %.*s\n", str8_fmt(ctx->recv_buf));
    return;
  };

  String8List message_list = {0};
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_ehlo(scratch.arena, ctx->conf.addr));
  str8_list_push(
      scratch.arena, &message_list,
      smtp_command_auth_plain(scratch.arena, ctx->username, ctx->password));
  str8_list_push(scratch.arena, &message_list,
                 smtp_command_from(scratch.arena, ctx->username));
  ForEachNode(recipient, String8Node, recipient_list.first) {
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
  ForEachNode(message, String8Node, message_list.first) {
    if (send_all_tls(smtp_ssl, message->string).status != Net_Status_Ok) {
      printf("Error when trying to send %.*s\n", str8_fmt(message->string));
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

    if (recv_until_crlf_tls(smtp_ssl, &ctx->recv_buf).status != Net_Status_Ok) {
      printf("recv failed\n");
      message_sent = 0;
      break;
    }

    SMTP_ReplyCode ok_code = RC_OK;
    if (str8_compare_cstring(str8_substr(message->string, r1u64(0, 4)),
                             "DATA")) {
      ok_code = RC_START_MAIL_INPUT;
      is_sending_data = 1;
    } else if (str8_compare_cstring(str8_substr(message->string, r1u64(0, 10)),
                                    "AUTH PLAIN")) {
      ok_code = RC_AUTH_SUCCESS;
      is_sending_data = 1;
    }

    if (smtp_get_reply_code(ctx->recv_buf) != ok_code) {
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
cleanup:;
  LogScopeResult log = log_scope_end(scratch.arena, LogMsgKind_All);
  append_data_to_file_path(ctx->conf.log_path, log.string);
  arena_scratch_release(scratch);
}
