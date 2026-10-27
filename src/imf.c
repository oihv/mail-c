extern String8 CRLF;

fn String8 imf_local_date(Arena* arena) {
  time_t rawtime = time(NULL);
  struct tm* ti; // time info
  ti = localtime(&rawtime);

  char buf[256] = {};
  strftime(buf, 256, "%a, %e %b %G %T %z", ti);

  String8 res = str8_cstring(buf);
  String8 res_owned = str8_copy(arena, res);
  return res_owned;
}

fn String8 imf_message_compose(Arena* arena, String8 date, String8 from,
                               String8 data) {
  Temp scratch = arena_scratch_get(&arena, 1);
  String8List list = {0};
  String8 date_head =
      str8f(scratch.arena, "Date: %.*s%.*s", str8_fmt(date), str8_fmt(CRLF));
  str8_list_push(scratch.arena, &list, date_head);
  String8 from_head =
      str8f(scratch.arena, "From: %.*s%.*s", str8_fmt(from), str8_fmt(CRLF));
  str8_list_push(scratch.arena, &list, from_head);
  str8_list_push(scratch.arena, &list, CRLF); // blank crlf
  str8_list_push(
      scratch.arena, &list,
      data); // TODO: there's a limit of 78 (should) and 998 (must)
             // for each line in IMF
             // NOTE: data is assumed to already be CRLF terminated each line
  String8 end = str8f(scratch.arena, ".%.*s", str8_fmt(CRLF));
  str8_list_push(scratch.arena, &list, end);
  // StringJoin join = {.post = str8_lit(".\r\n")};

  String8 res = str8_list_join(arena, &list, 0);
  arena_scratch_release(scratch);
  return res;
}
