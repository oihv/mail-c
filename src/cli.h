#ifndef CLI_H
#define CLI_H

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
  Config conf;
  int pop3_fd;
  String8 recv_buf;
  String8 username;
} CLI;

// Large enough for the biggest single input line (the recipient list, which is
// MAX_RECIPIENT addresses of up to MAX_USERNAME_LENGTH bytes separated by ';').
#define CLI_INPUT_CAP 4096
#define MAX_USERNAME_LENGTH 256
#define MAX_PASS_LENGTH 256

fn String8 cli_read_line(Arena* arena);
fn void cli_login(Arena* arena, CLI* ctx);
fn void cli_menu(CLI* ctx);
fn void cli_inbox(CLI* ctx);
fn void cli_compose(CLI* ctx);

#endif
