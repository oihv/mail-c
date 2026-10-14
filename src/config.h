#ifndef CONFIG_H
#define CONFIG_H

// NOTE: String should live in the arena that is provided to config_from_file
typedef struct {
  String8 addr;
  String8 smtp_port;
  String8 pop3_port;
  String8 username;
  String8 db_path;
  String8 server_log_path;
  String8 client_log_path;
} Config;

#endif
