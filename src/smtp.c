fn String8 smtp_reply_format_line(Arena* arena, SMTP_ReplyCode rc, String8 text) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 rc_str = str8f(scratch.arena, "%d ", rc);
  String8 content = str8_concat(scratch.arena, rc_str, text);
  String8 res = str8_concat(arena, content, CRLF);
  arena_scratch_release(scratch);
  return res;
}

// TODO: multiline format (adding hyphen "-" in between the reply code and text)

// ----- SMTP reply functions
// NOTE: expects hostname to not have parentheses
fn String8 smtp_reply_ready(Arena* arena, String8 hostname) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 hostname_parentheses =
      str8f(scratch.arena, "(%.*s)", str8_fmt(hostname));
  String8 content =
      str8_concat(scratch.arena, hostname_parentheses,
                  str8_lit(" Simple Mail Transfer Protocol Ready"));
  String8 res = smtp_reply_format_line(arena, RC_SERVICE_READY, content);

  arena_scratch_release(scratch);
  return res;
}

fn String8 smtp_reply_closing(Arena* arena, String8 hostname) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 hostname_parentheses =
      str8f(scratch.arena, "(%s)", (char*)hostname.str);
  String8 content =
      str8_concat(scratch.arena, hostname_parentheses,
                  str8_lit(" Service closing transmission channel"));
  String8 res =
      smtp_reply_format_line(arena, RC_CLOSING_TRANSMISSION_CHANNEL, content);

  arena_scratch_release(scratch);
  return res;
}

// TODO: include extensions and greet when first receive EHLO
fn String8 smtp_reply_ok(Arena* arena) {
  String8 res = smtp_reply_format_line(arena, RC_OK, str8_lit("OK"));
  return res;
}

fn String8 smtp_reply_start_mail_input(Arena* arena) {
  String8 res = smtp_reply_format_line(
      arena, RC_START_MAIL_INPUT,
      str8_lit("Start mail input; end with <CRLF>.<CRLF>"));
  return res;
}

fn String8 smtp_reply_service_not_avail(Arena* arena) {
  String8 res = smtp_reply_format_line(arena, RC_SERVICE_NOT_AVAIL,
                                       str8_lit("Service is not implemented"));
  return res;
}

fn String8 smtp_reply_unknown_command(Arena* arena) {
  String8 res = smtp_reply_format_line(arena, RC_UNKNOWN_COMMAND,
                                       str8_lit("Unknown command"));
  return res;
}

fn String8 smtp_reply_syntax_error(Arena* arena) {
  String8 res =
      smtp_reply_format_line(arena, RC_SYNTAX_ERROR, str8_lit("Syntax error"));
  return res;
}

fn String8 smtp_reply_recipient_rejected(Arena* arena) {
  String8 res =
      smtp_reply_format_line(arena, RC_REJECT, str8_lit("Recipient rejected"));
  return res;
}

fn SMTP_ReplyCode smtp_get_reply_code(String8 reply) {
  String8 code = str8_substr(reply, r1u64(0, 3));
  SMTP_ReplyCode res = (SMTP_ReplyCode)str8_to_i32(code);
  return res;
}

// ----- SMTP command functions
// NOTE: expects hostname to not have parentheses
fn String8 smtp_command_ehlo(Arena* arena, String8 rcpt) {
  String8 res = str8f(arena, "EHLO (%.*s)%.*s", str8_fmt(rcpt), str8_fmt(CRLF));
  return res;
}

fn String8 smtp_command_from(Arena* arena, String8 from) {
  String8 res =
      str8f(arena, "MAIL FROM: <%.*s>%.*s", str8_fmt(from), str8_fmt(CRLF));
  return res;
}

fn String8 smtp_command_to(Arena* arena, String8 to) {
  String8 res =
      str8f(arena, "RCPT TO: <%.*s>%.*s", str8_fmt(to), str8_fmt(CRLF));
  return res;
}

fn String8 smtp_command_data(Arena* arena) {
  String8 res = str8f(arena, "DATA%.*s", str8_fmt(CRLF));
  return res;
}

fn String8 smtp_command_quit(Arena* arena) {
  String8 res = str8f(arena, "QUIT%.*s", str8_fmt(CRLF));
  return res;
}

fn String8 smtp_command_auth_plain(Arena* arena, String8 full_username, String8 password) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 credential = str8f(scratch.arena, "%c%.*s%c%.*s", '\0', str8_fmt(full_username), '\0', str8_fmt(password)); // use %c otherwise sprintf read \0 as terminator
  String8 credential_encoded = base64_encode(scratch.arena, credential);
  String8 res = str8f(arena, "AUTH PLAIN %.*s%.*s", str8_fmt(credential_encoded), str8_fmt(CRLF));
  arena_scratch_release(scratch);
  return res;
}

fn String8 smtp_process_line(Arena* arena, Session* state, String8 line) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 reply = {0};

  // parse the command
  if (state->reading_data) {
    if (line.size == 1 && line.str[0] == '.') {
      log_infof(
          "[t%u] [info] DATA termination received. Stopped reading data\n",
          log_thread_id);
      state->reading_data = false;
      String8 data_joined = str8_list_join(
          state->arena, &state->data, &(StringJoin){.sep = str8_lit("\n")});
      // StrEvalPrint(data_joined);
      db_store_mail(state->db, state->sender, &state->recipients, data_joined);
      log_infof("[t%u] [info] Mail stored into db, to sender %.*s\n",
                log_thread_id, str8_fmt(state->sender));
    } else {
      log_infof("[t%u] [info] reading data...\n", log_thread_id);
      String8 data_line = str8_copy(state->arena, line);
      str8_list_push(state->arena, &state->data, data_line);
    }

  } else if (str8_compare(str8_substr(line, r1u64(0, 4)), str8_lit("EHLO"))) {
    log_infof("[t%u] [info] read EHLO command\n", log_thread_id);
    state->prev_command = SMTP_EHLO;
    reply = smtp_reply_ok(arena);

  } else if (str8_compare(str8_substr(line, r1u64(0, 9)),
                          str8_lit("MAIL FROM"))) {
    log_infof("[t%u] [info] read MAIL FROM command\n", log_thread_id);
    if (state->prev_command != SMTP_EHLO) {
      log_user_errorf("[t%u] [error] client haven't sent EHLO command\n",
                      log_thread_id);
      reply = smtp_reply_syntax_error(arena);
    }

    state->prev_command = SMTP_MAIL_FROM;

    // get the sender
    String8List list = str8_split(scratch.arena, line, str8_lit("<"));
    if (list.node_count != 2) {
      reply = smtp_reply_syntax_error(arena);
      log_user_errorf("[t%u] [error] MAIL FROM command got dubious syntax\n",
                      log_thread_id);
    } else {
      String8 sender = parse_mail_address(list.last->string);
      state->sender = str8_copy(state->arena, sender);
      log_infof("[t%u] [info] Successfully parsed sender address: %.*s\n",
                log_thread_id, str8_fmt(sender));

      reply = smtp_reply_ok(arena);
    }

  } else if (str8_compare(str8_substr(line, r1u64(0, 7)),
                          str8_lit("RCPT TO"))) {
    log_infof("[t%u] [info] read RCPT TO command\n", log_thread_id);
    if (state->prev_command != SMTP_MAIL_FROM &&
        state->prev_command != SMTP_RCPT_TO) {
      log_user_errorf("[t%u] [error] MAIL FROM command got dubious syntax\n",
                      log_thread_id);
      reply = smtp_reply_syntax_error(arena);
    }
    state->prev_command = SMTP_RCPT_TO;

    // get the recipient
    String8List list = str8_split(scratch.arena, line, str8_lit("<"));
    if (list.node_count != 2) {
      log_user_errorf("[t%u] [error] RCPT TO command got dubious syntax\n",
                      log_thread_id);
      reply = smtp_reply_syntax_error(arena);
    } else {
      String8 recipient = parse_mail_address(list.last->string);
      recipient = str8_copy(state->arena, recipient);
      str8_list_push(state->arena, &state->recipients, recipient);
      log_infof("[t%u] [info] Successfully parsed recipient address: %.*s\n",
                log_thread_id, str8_fmt(recipient));
      reply = smtp_reply_ok(arena);
    }

  } else if (str8_compare(str8_substr(line, r1u64(0, 4)), str8_lit("DATA"))) {
    log_infof("[t%u] [info] read DATA command\n", log_thread_id);
    state->prev_command = SMTP_DATA;
    state->reading_data = true;
    log_infof("[t%u] [info] start reading DATA...\n", log_thread_id);
    reply = smtp_reply_start_mail_input(arena);

  } else if (str8_compare(str8_substr(line, r1u64(0, 4)), str8_lit("QUIT"))) {
    log_infof("[t%u] [info] read QUIT command, quitting...\n", log_thread_id);
    state->prev_command = SMTP_QUIT;
    reply = smtp_reply_closing(arena, state->conf->addr);
  }

  arena_scratch_release(scratch);
  return reply;
}
