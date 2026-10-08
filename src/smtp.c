#include "base/base.h"
typedef enum {
  RC_SERVICE_READY = 220,
  RC_CLOSING_TRANSMISSION_CHANNEL = 221,
  RC_OK = 250,
  RC_START_MAIL_INPUT = 354,
  RC_SERVICE_NOT_AVAIL = 421,
  RC_UNKNOWN_COMMAND = 500,
  RC_SYNTAX_ERROR = 501,
  RC_REJECT = 550,
} SMTP_ReplyCode;

typedef enum {
  SMTP_EHLO,
  SMTP_MAIL_FROM,
  SMTP_RCPT_TO,
  SMTP_DATA,
  SMTP_QUIT,
} SMTP_Command;

String8 CRLF = (String8){.str = (u8 *)"\r\n", .size = 2};

String8 smtp_reply_format_line(Arena* arena, SMTP_ReplyCode rc, String8 text) {
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
String8 smtp_reply_ready(Arena *arena, String8 hostname) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 hostname_parentheses = str8f(scratch.arena, "(%.*s)", str8_fmt(hostname));
  String8 content = str8_concat(scratch.arena, hostname_parentheses, str8_lit(" Simple Mail Transfer Protocol Ready"));
  String8 res = smtp_reply_format_line(arena, RC_SERVICE_READY, content);

  arena_scratch_release(scratch);
  return res;
}

String8 smtp_reply_closing(Arena *arena, String8 hostname) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8 hostname_parentheses = str8f(scratch.arena, "(%s)", (char*) hostname.str);
  String8 content = str8_concat(scratch.arena, hostname_parentheses, str8_lit(" Service closing transmission channel"));
  String8 res = smtp_reply_format_line(arena, RC_CLOSING_TRANSMISSION_CHANNEL, content);

  arena_scratch_release(scratch);
  return res;
}

// TODO: include extensions and greet when first receive EHLO
String8 smtp_reply_ok(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_OK, str8_lit("OK"));
  return res;
}

String8 smtp_reply_start_mail_input(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_START_MAIL_INPUT, str8_lit("Start mail input; end with <CRLF>.<CRLF>"));
  return res;
}

String8 smtp_reply_service_not_avail(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_SERVICE_NOT_AVAIL, str8_lit("Service is not implemented"));
  return res;
}

String8 smtp_reply_unknown_command(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_UNKNOWN_COMMAND, str8_lit("Unknown command"));
  return res;
}

String8 smtp_reply_syntax_error(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_SYNTAX_ERROR, str8_lit("Syntax error"));
  return res;
}

String8 smtp_reply_recipient_rejected(Arena *arena) {
  String8 res = smtp_reply_format_line(arena, RC_REJECT, str8_lit("Recipient rejected"));
  return res;
}

// ----- SMTP command functions
// NOTE: expects hostname to not have parentheses
String8 smtp_command_ehlo(Arena* arena, String8 rcpt) {
  String8 res = str8f(arena, "EHLO (%.*s)", str8_fmt(rcpt));
  String8 res_crlf = str8_concat(arena, res, CRLF);
  return res_crlf;
}

String8 smtp_command_from(Arena* arena, String8 from) {
  String8 res = str8f(arena, "MAIL FROM: <%.*s>", str8_fmt(from));
  String8 res_crlf = str8_concat(arena, res, CRLF);
  return res_crlf;
}

String8 smtp_command_to(Arena* arena, String8 to) {
  String8 res = str8f(arena, "RCPT TO: <%.*s>", str8_fmt(to));
  String8 res_crlf = str8_concat(arena, res, CRLF);
  return res_crlf;
}

String8 smtp_command_data(Arena* arena) {
  String8 res = str8_lit("DATA");
  String8 res_crlf = str8_concat(arena, res, CRLF);
  return res_crlf;
}

String8 smtp_command_quit(Arena* arena) {
  String8 res = str8_lit("QUIT");
  String8 res_crlf = str8_concat(arena, res, CRLF);
  return res_crlf;
}

void smtp_process_command(String8 cmd) {
  // parse the command
  // take action based on the command (decode the command)
  // NOTE: what if there's error? how should error be propagated?
}
