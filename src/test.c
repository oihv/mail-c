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

void test_assert(String8 msg, int x) {
  printf("%.*s... ", str8_fmt(msg));
  if (x) {
    printf("OK\n");
  } else {
    printf("ERR\n");
  }
}

int main() {
  Temp scratch = arena_scratch_get(0, 0);

  // config.c
  {
    Config conf = config_parse(scratch.arena,
                               str8_lit("addr = 0\nsmtp_port = 1025\n"));
    test_assert(str8_lit("config_parse addr"),
                str8_compare_cstring(conf.addr, "0"));
    test_assert(str8_lit("config_parse username"),
                str8_compare_cstring(conf.smtp_port, "1025"));
  }

  // smtp.c
  {
    {
      String8 res = smtp_reply_format_line(scratch.arena, RC_SERVICE_READY,
                                           str8_lit("Wocao"));
      String8 expect = str8_lit("220 Wocao\r\n");
      test_assert(str8_lit("smtp_format_line"), str8_compare(res, expect));
    }
    {
      String8 res = smtp_reply_ready(scratch.arena, str8_lit("0.0.0.0"));
      String8 expect =
          str8_lit("220 (0.0.0.0) Simple Mail Transfer Protocol Ready\r\n");
      test_assert(str8_lit("smtp_reply_ready"), str8_compare(res, expect));
    }
    {
      String8 res = smtp_reply_closing(scratch.arena, str8_lit("0.0.0.0"));
      String8 expect =
          str8_lit("221 (0.0.0.0) Service closing transmission channel\r\n");
      test_assert(str8_lit("smtp_reply_closing"), str8_compare(res, expect));
    }
    {
      String8 res = smtp_reply_ok(scratch.arena);
      String8 expect = str8_lit("250 OK\r\n");
      test_assert(str8_lit("smtp_reply_ok"), str8_compare(res, expect));
    }
    {
      String8 res = smtp_reply_closing(scratch.arena, str8_lit("0.0.0.0"));
      String8 expect =
          str8_lit("221 (0.0.0.0) Service closing transmission channel\r\n");
      test_assert(str8_lit("smtp_reply_closing"), str8_compare(res, expect));
    }
    {
      String8 res = parse_mail_address(str8_lit("t@t.com>"));
      String8 expect = str8_lit("t@t.com");
      test_assert(str8_lit("parse_mail_address"), str8_compare(res, expect));
      StrEvalPrint(res);
    }
    {
      String8 data = str8_lit("This is just a message\r\n");
      String8 from = str8_lit("okay@then.com");
      String8 to = str8_lit("ben@bit.com");
      String8 imf_res = imf_message_compose(
          scratch.arena, imf_local_date(scratch.arena), from, to, data);
      StrEvalPrint(imf_res);
    }
  }
  arena_scratch_release(scratch);
  scratch = arena_scratch_get(0, 0);
}
