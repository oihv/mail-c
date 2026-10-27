// TODO: error handling for all functions

sqlite3* db_open(String8 path) {
  // sql open db
  sqlite3* db = NULL;
  sqlite3_open((char*)path.str, &db);

  // Every worker thread owns its own connection to the same file. With the
  // default settings a second writer gets SQLITE_BUSY ("database is locked")
  // immediately, and sqlite3_exec just returns that error - the write is lost.
  // A busy handler makes SQLite wait for the lock instead of failing.
  sqlite3_busy_timeout(db, 5000);

  // WAL lets readers (LIST/STAT) run concurrently with the single writer
  // instead of blocking each other on a rollback journal.
  sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);

  sqlite3_exec(db,
               "CREATE TABLE IF NOT EXISTS accounts ("
               "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  username  TEXT,"
               "  password TEXT"
               ")",
               NULL, NULL, NULL);
  sqlite3_exec(db,
               "CREATE TABLE IF NOT EXISTS mails ("
               "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  sender  TEXT,"
               "  mail TEXT,"
               "  octets INTEGER"
               ")",
               NULL, NULL, NULL);
  sqlite3_exec(db,
               "CREATE TABLE IF NOT EXISTS recipients ("
               "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  mail_id INTEGER,"
               "  address TEXT"
               ")",
               NULL, NULL, NULL);
  return db;
}

i32 db_get_username_id(sqlite3* db, String8 username) {
  Temp scratch = arena_scratch_get(0, 0);
  sqlite3_stmt* st;
  i32 id = -1;
  String8 query =
      str8f(scratch.arena, "SELECT id FROM accounts WHERE username=\"%.*s\"",
            str8_fmt(username));

  sqlite3_prepare_v2(db, (char*)query.str, -1, &st, NULL);
  if (sqlite3_step(st) == SQLITE_ROW) {
    id = sqlite3_column_int(st, 0);
  }
  sqlite3_finalize(st);

  arena_scratch_release(scratch);
  return id;
}

// arena to store the password
String8 db_get_password_for_username(Arena* arena, sqlite3* db,
                                     String8 username) {
  Temp scratch = arena_scratch_get(&arena, 1);
  sqlite3_stmt* st;
  String8 pass = {0};
  String8 query = str8f(scratch.arena,
                        "SELECT password FROM accounts WHERE username=\"%.*s\"",
                        str8_fmt(username));

  sqlite3_prepare_v2(db, (char*)query.str, -1, &st, NULL);
  if (sqlite3_step(st) == SQLITE_ROW) {
    pass = str8_cstring((char*)sqlite3_column_text(st, 0));
    pass = str8_copy(arena, pass); // copy to arena, since the string from
                                   // sqlite will be invalidated after finalize
  }
  sqlite3_finalize(st);

  arena_scratch_release(scratch);
  return pass; // NOTE: pass = {0} indicates user not found
}

b32 db_check_username_registered(sqlite3* db, String8 username) {
  Temp scratch = arena_scratch_get(0, 0);
  b32 exists = 1;
  String8 pass = db_get_password_for_username(scratch.arena, db, username);
  if (pass.size == 0) {
    exists = 0;
  }

  arena_scratch_release(scratch);
  return exists;
}

void db_store_mail(sqlite3* db, String8 sender, String8List* recipients,
                   String8 data) {
  Temp scratch = arena_scratch_get(0, 0);
  String8 query_insert_mail =
      str8f(scratch.arena,
            "INSERT INTO mails (sender, mail, octets) VALUES "
            "( \"%.*s\", \"%.*s\", %d )",
            str8_fmt(sender), str8_fmt(data), data.size);
  StrEvalPrint(query_insert_mail);
  i32 rc = sqlite3_exec(db, (char*)query_insert_mail.str, NULL, NULL, NULL);
  if (rc != SQLITE_OK) {
    log_user_errorf("[error] failed to insert mail: %s\n", sqlite3_errmsg(db));
  }
  arena_scratch_release(scratch);

  i64 mail_id = sqlite3_last_insert_rowid(db);
  ForEachNode(recipient, String8Node, recipients->first) {
      scratch = arena_scratch_get(0, 0);
      String8 query_insert_recipient =
          str8f(scratch.arena,
                "INSERT INTO recipients (mail_id, address) VALUES "
                "( %d, \"%.*s\" )",
                mail_id, str8_fmt(recipient->string));
      i32 rc_recipient =
          sqlite3_exec(db, (char*)query_insert_recipient.str, NULL, NULL, NULL);
      if (rc_recipient != SQLITE_OK) {
        log_user_errorf("[error] failed to insert recipient: %s\n",
                        sqlite3_errmsg(db));
      }
      arena_scratch_release(scratch);
    }

  arena_scratch_release(scratch);
  return;
}

String8 db_get_mail(Arena* arena, sqlite3* db, u64 mail_id) {
  Temp scratch = arena_scratch_get(0, 0);
  sqlite3_stmt* st;

  String8 query = str8f(
      scratch.arena, "SELECT mail FROM mails WHERE id=%lu",
      mail_id);
  sqlite3_prepare_v2(db, (char*)query.str, -1, &st, NULL);
  String8 res = {0};
  if (sqlite3_step(st) == SQLITE_ROW) {
    res = str8_cstring((char*)sqlite3_column_text(st, 0));
    res = str8_copy(arena, res);
  }
  sqlite3_finalize(st);

  arena_scratch_release(scratch);
  return res;
}

u64 db_get_mails_count(sqlite3* db, String8 recipient) {
  Temp scratch = arena_scratch_get(0, 0);
  sqlite3_stmt* st;

  String8 query_count = str8f(
      scratch.arena, "SELECT COUNT(*) FROM recipients WHERE address=\"%.*s\"",
      str8_fmt(recipient));
  sqlite3_prepare_v2(db, (char*)query_count.str, -1, &st, NULL);
  u64 count = 0;
  if (sqlite3_step(st) == SQLITE_ROW) {
    count = sqlite3_column_int(st, 0);
  }
  sqlite3_finalize(st);

  arena_scratch_release(scratch);
  return count;
}

u64 db_get_mails_total_octets(sqlite3* db, String8 recipient) {
  Temp scratch1 = arena_scratch_get(0, 0);
  sqlite3_stmt* st1;

  String8 query_recipient = str8f(
      scratch1.arena, "SELECT mail_id FROM recipients WHERE address=\"%.*s\"",
      str8_fmt(recipient));
  sqlite3_prepare_v2(db, (char*)query_recipient.str, -1, &st1, NULL);

  u64 total_octets = 0;
  while (sqlite3_step(st1) == SQLITE_ROW) {
    Temp scratch2 = arena_scratch_get(&scratch1.arena, 1);
    i64 mail_id = sqlite3_column_int(st1, 0);

    sqlite3_stmt* st2;
    String8 query_octet =
        str8f(scratch2.arena, "SELECT octets FROM mails WHERE id=%d", mail_id);
    sqlite3_prepare_v2(db, (char*)query_octet.str, -1, &st2, NULL);
    if (sqlite3_step(st2) == SQLITE_ROW) {
      i64 octet = sqlite3_column_int(st2, 0);
      total_octets += octet;
    }
    sqlite3_finalize(st2);

    arena_scratch_release(scratch2);
  }
  sqlite3_finalize(st1);

  arena_scratch_release(scratch1);
  return total_octets;
}

i64Array db_get_array_mail_octets(Arena* arena, sqlite3* db, String8 recipient) {
  Temp scratch1 = arena_scratch_get(0, 0);
  sqlite3_stmt* st1;

  u64 count = db_get_mails_count(db, recipient);

  String8 query_recipient = str8f(
      scratch1.arena, "SELECT mail_id FROM recipients WHERE address=\"%.*s\"",
      str8_fmt(recipient));
  sqlite3_prepare_v2(db, (char*)query_recipient.str, -1, &st1, NULL);

  i64Array res = {.v = arena_push(arena, sizeof(i64) * count, 1)};
  while (sqlite3_step(st1) == SQLITE_ROW) {
    Temp scratch2 = arena_scratch_get(&scratch1.arena, 1);
    i64 mail_id = sqlite3_column_int(st1, 0);

    sqlite3_stmt* st2;
    String8 query_octet =
        str8f(scratch2.arena, "SELECT octets FROM mails WHERE id=%d", mail_id);
    sqlite3_prepare_v2(db, (char*)query_octet.str, -1, &st2, NULL);
    if (sqlite3_step(st2) == SQLITE_ROW) {
      i64 octet = sqlite3_column_int(st2, 0);
      res.v[res.count++] = octet;
    }
    sqlite3_finalize(st2);

    arena_scratch_release(scratch2);
  }
  sqlite3_finalize(st1);

  arena_scratch_release(scratch1);
  return res;
}

i64Array db_get_array_mail_ids(Arena* arena, sqlite3* db, String8 recipient) {
  Temp scratch1 = arena_scratch_get(0, 0);
  sqlite3_stmt* st1;

  u64 count = db_get_mails_count(db, recipient);

  String8 query_recipient = str8f(
      scratch1.arena, "SELECT mail_id FROM recipients WHERE address=\"%.*s\"",
      str8_fmt(recipient));
  sqlite3_prepare_v2(db, (char*)query_recipient.str, -1, &st1, NULL);

  i64Array res = {.v = arena_push(arena, sizeof(i64) * count, 1)};
  while (sqlite3_step(st1) == SQLITE_ROW) {
    Temp scratch2 = arena_scratch_get(&scratch1.arena, 1);
    i64 mail_id = sqlite3_column_int(st1, 0);

    sqlite3_stmt* st2;
    String8 query_octet =
        str8f(scratch2.arena, "SELECT id FROM mails WHERE id=%d", mail_id); // extra query because of ordering
    sqlite3_prepare_v2(db, (char*)query_octet.str, -1, &st2, NULL);
    if (sqlite3_step(st2) == SQLITE_ROW) {
      i64 id = sqlite3_column_int(st2, 0);
      res.v[res.count++] = id;
    }
    sqlite3_finalize(st2);

    arena_scratch_release(scratch2);
  }
  sqlite3_finalize(st1);

  arena_scratch_release(scratch1);
  return res;
}
