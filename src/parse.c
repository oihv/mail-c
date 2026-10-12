#include "base/base.h"
#include "threadpool.h"

// syntax: <mail@domain.xyz>
String8 parse_mail_address(String8 s) {
  String8 email_raw = str8_skip_chop_whitespace(s);
  u64 email_end = 0;
  bool add = false;
  for (int i = 0; i < email_raw.size; i++) {
    if (email_raw.str[i] == '@') {
      if (!add)
        add = true;
      else {
        log_user_error(str8_lit("[error] email address got more than one '@'\n"));
        break;
      }
    }
    if (email_raw.str[i] == '>') {
      break;
    }
    email_end++;
  }

  if (email_end != email_raw.size - 1) {
    log_user_error(str8_lit("[error] got unexpected token after email address\n"));
  }

  String8 sender = str8_substr(email_raw, r1u64(0, email_end));
}
