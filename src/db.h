#ifndef DB_H
#define DB_H

#include <sqlite3.h>

sqlite3* db_open(String8 path);
i32 db_get_username_id(sqlite3* db, String8 username);
String8 db_get_password_for_username(Arena* arena, sqlite3* db,
                                     String8 username);
b32 db_check_username_registered(sqlite3* db, String8 username);
void db_store_mail(sqlite3* db, String8 sender, String8List* recipients,
                   String8 data);
String8 db_get_mail(Arena* arena, sqlite3* db, u64 mail_id);
u64 db_get_mails_count(sqlite3* db, String8 recipient);
u64 db_get_mails_total_octets(sqlite3* db, String8 recipient);
i64Array db_get_array_mail_octets(Arena* arena, sqlite3* db, String8 recipient);
i64Array db_get_array_mail_ids(Arena* arena, sqlite3* db, String8 recipient);

#endif
