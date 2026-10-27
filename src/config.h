#ifndef CONFIG_H
#define CONFIG_H

// NOTE: String should live in the arena that is provided to config_from_file
#define CONFIG_FIELDS(X) \
    X(addr)              \
    X(smtp_port)         \
    X(pop3_port)         \
    X(db_path)           \
    X(log_path)

typedef struct {
#define X(name) String8 name;
    CONFIG_FIELDS(X)
#undef X
} Config;

#define X(name) str8_const_lit(#name),
static const String8 _config_fields[] = { CONFIG_FIELDS(X) };
#undef X

#define X(name) offsetof(Config, name),
static const size_t _config_offsets[] = { CONFIG_FIELDS(X) };
#undef X

enum { CONFIG_FIELD_NUM = sizeof(_config_fields) / sizeof(_config_fields[0]) };

Config config_default();
Config config_parse(Arena* arena, String8 data);
Config config_from_file(Arena* arena, String8 path);

#endif
