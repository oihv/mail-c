#ifndef SMTP_H
#define SMTP_H

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

typedef struct {
  Arena* arena;
  Temp temp;
  String8 sender;   // for sending Mail
  String8 username; // for authentication
  String8List recipients;
  String8List data;
  Config* conf;
  b32 reading_data;
  SMTP_Command prev_command;
  sqlite3* db;
  i64Array mail_ids;
  i64Array octets;
} Session;

String8 CRLF = (String8){.str = (u8*)"\r\n", .size = 2};
fn String8 smtp_reply_format_line(Arena* arena, SMTP_ReplyCode rc, String8 text);
fn String8 smtp_reply_ready(Arena* arena, String8 hostname);
fn String8 smtp_reply_closing(Arena* arena, String8 hostname);
fn String8 smtp_reply_ok(Arena* arena);
fn String8 smtp_reply_start_mail_input(Arena* arena);
fn String8 smtp_reply_service_not_avail(Arena* arena);
fn String8 smtp_reply_unknown_command(Arena* arena);
fn String8 smtp_reply_syntax_error(Arena* arena);
fn String8 smtp_reply_recipient_rejected(Arena* arena);
fn SMTP_ReplyCode smtp_get_reply_code(String8 reply);
fn String8 smtp_command_ehlo(Arena* arena, String8 rcpt);
fn String8 smtp_command_from(Arena* arena, String8 from);
fn String8 smtp_command_to(Arena* arena, String8 to);
fn String8 smtp_command_data(Arena* arena);
fn String8 smtp_command_quit(Arena* arena);
fn String8 smtp_process_line(Arena* arena, Session* state, String8 line);

#endif 
