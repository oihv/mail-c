Config config_default() {
  Config out = {0};

  out.addr = str8_lit("0");
  out.smtp_port = str8_lit("1025");
  out.pop3_port = str8_lit("1110");
  out.db_path = str8_lit("database.db");
  out.log_path = str8_lit(".log");

  return out;
}

Config config_parse(Arena* arena, String8 data) {
  Temp scratch = arena_scratch_get(&arena, 1);

  Config res = config_default();
  String8 newline = str8_lit("\n");
  String8List lines = str8_split(scratch.arena, data, newline);

  String8 equal = str8_lit("=");
  u64 field_set_count[CONFIG_FIELD_NUM] = {0};
  ForEachNode(line, String8Node, lines.first) {
    String8List pair = str8_split(scratch.arena, line->string, equal);
    String8 key = str8_skip_chop_whitespace(pair.first->string);
    String8 val = str8_skip_chop_whitespace(pair.last->string);

    String8 scratch_str = str8_copy(arena, val);
    u64 field_idx = 0;
    b32 found = 0;
    ForEachIndex(i, CONFIG_FIELD_NUM) {
      if (str8_compare(key, _config_fields[i])) {
        String8* field = (String8*)((char*)&res + _config_offsets[i]);
        *field = scratch_str;
        field_idx = i;
        found = 1;
        break;
      }
    }
    // if (str8_compare(key, _config_fields[0])) {
    //   res.addr = scratch_str;
    //   field_idx = 0;
    // } else if (str8_compare(key, _config_fields[1])) {
    //   // TODO: check max value
    //   res.smtp_port = scratch_str;
    //   field_idx = 1;
    // } else if (str8_compare(key, _config_fields[2])) {
    //   // TODO: check max value
    //   res.pop3_port = scratch_str;
    //   field_idx = 2;
    // } else if (str8_compare(key, _config_fields[3])) {
    //   res.username = scratch_str;
    //   field_idx = 3;
    // } else if (str8_compare(key, _config_fields[4])) {
    //   res.db_path = scratch_str;
    //   field_idx = 4;
    //   // null terminate because this will be used by sqlite
    //   // sqlite uses null terminated string
    //   scratch_str.str[temp_str.size] = 0;
    // } else if (str8_compare(key, _config_fields[5])) {
    //   res.server_log_path = scratch_str;
    //   field_idx = 5;
    // } else if (str8_compare(key, _config_fields[6])) {
    //   res.client_log_path = scratch_str;
    //   field_idx = 6;
    // } else {
    //   found = 0;
    //   log_warnf("when reading config: key %.*s not recognized\n",
    //             str8_fmt(key));
    // }

    if (found)
      field_set_count[field_idx] += 1;
    else
      log_warnf("when reading config: key %.*s not recognized\n",
                str8_fmt(key));
  }

  // check if there's a field set twice
  ForEachIndex(i, CONFIG_FIELD_NUM) {
    if (field_set_count[i] > 1)
      log_warnf("field '%.*s' is set more than once\n",
                str8_fmt(_config_fields[i]));
  }

  arena_scratch_release(scratch);
  return res;
}

Config config_from_file(Arena* arena, String8 path) {
  Config out = {0};
  FILE* fp = fopen((char*)path.str, "r");
  if (fp == 0) {
    log_user_errorf("[error] when reading config at fopen: %s\n",
                    strerror(errno));
    log_infof("[info] using default config\n", strerror(errno));
    return out;
  }

  Temp temp = arena_scratch_get(0, 0);

  u64 buff_size = MB(1);
  char* buff = arena_push(temp.arena, buff_size, true);

  fread(buff, sizeof(*buff), buff_size, fp);

  String8 buff_str = str8((u8*)buff, strlen(buff));

  out = config_parse(arena, buff_str);

  log_infof("[info] config file loaded from %.*s\n", str8_fmt(path));

  arena_scratch_release(temp);
  return out;
}
