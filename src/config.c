#include <stdcountof.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include "config.h"
#include "base/base.h"

Config config_from_file(Arena* arena, String8 path) {
  Config res = {};
  res.server_log_path = str8_lit("server.log");
  res.client_log_path = str8_lit("client.log");

  FILE* fp = fopen((char *)path.str, "r");
  if (fp == 0) {
    log_user_errorf("[error] when reading config at fopen: %s\n", strerror(errno));
    return res;
  }

  Temp temp = arena_scratch_get(0, 0);
  u64 buff_size = MB(1);
  char* buff = arena_push(temp.arena, buff_size, true);
  fread(buff, sizeof(*buff), buff_size, fp);

  String8 buff_str = str8((u8 *) buff, strlen(buff));
  String8 newline = str8_lit("\n");
  String8List lines = str8_split(temp.arena, buff_str, newline);

  String8 equal = str8_lit("=");
  for EachNode(line, String8Node, lines.first) {
    String8List pair = str8_split(temp.arena, line->string, equal);
    String8 key = str8_skip_chop_whitespace(pair.first->string);
    String8 val = str8_skip_chop_whitespace(pair.last->string);

    u8* temp_c = arena_push(arena, (val.size + 1) * sizeof(u8), 1); // +1 for null terminator
    memcpy(temp_c, val.str, val.size);
    String8 temp_str = str8_cstring((char*)temp_c);
    if (str8_compare_cstring(key, "address")) {
      res.addr = temp_str;
    } else if (str8_compare_cstring(key, "smtp_port")) {
      // TODO: check max value
      res.smtp_port = temp_str;
    } else if (str8_compare_cstring(key, "pop3_port")) {
      // TODO: check max value
      res.pop3_port = temp_str;
    } else if (str8_compare_cstring(key, "username")) {
      res.username = temp_str;
    } else if (str8_compare_cstring(key, "db_path")) {
      res.db_path = temp_str;
      // null terminate because this will be used by sqlite
      // sqlite uses null terminated string
      temp_str.str[temp_str.size] = 0;
    } else if (str8_compare_cstring(key, "server_log_path")) {
      res.server_log_path = temp_str;
    } else if (str8_compare_cstring(key, "client_log_path")) {
      res.client_log_path = temp_str;
    } else {
      log_user_errorf("[error] when reading config: key %.*s not recognized\n", str8_fmt(key));
    }
  }

  temp_end(temp);

  log_infof("[info] config file loaded from %.*s\n", str8_fmt(path));

  return res;
}
