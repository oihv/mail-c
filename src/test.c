#include <assert.h>

#include "base/base.h"
#include "base/base.c"
#include "db.c"
#include "config.c"
__thread u32 log_thread_id = 0; // since some logs requires thread id
#include "smtp.c"
#include "parse.c"
#include "imf.c"

void test_assert(String8 msg, int x) {
  printf("%.*s... ", str8_fmt(msg));
  if (x) {
    printf("OK\n");
  } else {
    printf("ERR\n");
  }
}

int main() {
  // smtp.c
  Temp scratch = arena_scratch_get(0, 0);
  {
    {
      String8 res =
          smtp_reply_format_line(scratch.arena, RC_SERVICE_READY, str8_lit("Wocao"));
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
      String8 expect =
          str8_lit("250 OK\r\n");
      test_assert(str8_lit("smtp_reply_ok"), str8_compare(res, expect));
    }
    {
      String8 res = smtp_reply_closing(scratch.arena, str8_lit("0.0.0.0"));
      String8 expect =
          str8_lit("221 (0.0.0.0) Service closing transmission channel\r\n");
      test_assert(str8_lit("smtp_reply_closing"), str8_compare(res, expect));
    }
    {
      String8 data = str8_lit("This is just a message\r\n");
      String8 from = str8_lit("okay@then.com");
      String8 imf_res = imf_message_compose(scratch.arena, imf_local_date(scratch.arena), from, data);
      StrEvalPrint(imf_res);
    }
  }
  arena_scratch_release(scratch);
  scratch = arena_scratch_get(0, 0);
}
