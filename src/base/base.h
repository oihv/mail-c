#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
// declaration syntax
// #define internal static; // NOTE: what for?
// #define local static;
// #define global static;
#define fn static // will this be enough?

// (NT): standard types
#include <stdint.h>
typedef int8_t i8;
typedef int32_t i32;
typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;
typedef i8 b8;
typedef i32 b32;
// number helper
#define KB(n) (u64)(n << 10)
#define MB(n) (u64)(n << 20)
#define GB(n) (u64)(n << 30)

#define ForEachIndex(it, count) for (u64 it = 0; it < (count); it += 1) // 0..n-1
#define ForEachIndexRev(it, count)                                                \
  for (u64 it = (count); it > 0; it -= 1) // n..1 (not n-1..0 because otherwise unsigned number cant use this)
#define ForEachNode(it, T, first) for (T* it = first; it != 0; it = it->next)
#define ForEachEnumVal(type, it)                                                  \
  for (type it = (type)0; it < type##_COUNT; it = (type)(it + 1))

#include <stdio.h>
#define EvalPrint(x) printf("%s = %d\n", #x, (int)(x))

// (NT): memory operation
#include <stdlib.h>
#include <string.h>
#define MemoryZero(x, sz) memset((x), 0, (sz));
#define Malloc(x) malloc(x);
#define Free(x) free(x);

// Helper
#define Swap(T, a, b)                                                          \
  do {                                                                         \
    T t__ = a;                                                                 \
    a = b;                                                                     \
    b = t__;                                                                   \
  } while (0)

// math operations
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))
#define Max(a, b) ((a > b) ? a : b)
#define Min(a, b) ((a > b) ? b : a)
typedef union Rng1U64 Rng1U64;
union Rng1U64 {
  struct {
    u64 min;
    u64 max;
  };
  u64 v[2];
};

fn Rng1U64 r1u64(u64 min, u64 max);

typedef struct i64Array i64Array;
struct i64Array {
  i64* v;
  u64 count;
};

// (NT): arena allocator
typedef struct {
  u64 reserve_size;
  u64 commit_size;

  u64 pos;
  u64 commit_pos;
} Arena;

#define ARENA_BASE_POS (sizeof(Arena))
#define ARENA_ALIGN (sizeof(void*))

fn Arena* arena_alloc(u64 reserve_size, u64 commit_size);
fn void arena_release(Arena* arena);
fn void arena_set_auto_align(Arena* arena, u64 align);

fn u64 arena_pos(Arena* arena);

fn void* arena_push(Arena* arena, u64 size, b32 zero);
// fn void *arena_push_no_zero(Arena *arena, u64 size);
// fn void *arena_push_aligned(Arena *arena, u64 alignment);

fn void arena_pop(Arena* arena, u64 size);
fn void arena_pop_to(Arena* arena, u64 pos);
fn void arena_clear(Arena* arena);

// temp arena
// TODO: what for?
typedef struct {
  Arena* arena;
  u64 start_pos;
} Temp;

fn Temp temp_begin(Arena* arena);
fn void temp_end(Temp temp);

// scratch arena
fn Temp arena_scratch_get(Arena** conflicts, u32 num_conflicts);
fn void arena_scratch_release(Temp scratch);

fn u64 plat_get_pagesize(void);
fn void* plat_mem_reserve(u64 size);
fn b32 plat_mem_commit(void* ptr, u64 size);
fn b32 plat_mem_decommit(void* ptr, u64 size);
fn b32 plat_mem_release(void* ptr, u64 size);

// Nil check
#define CheckNil(nil, p) ((p) == 0 || (p) == (nil))
#define SetNil(nil, p) ((p) = (nil))

// Linked lists macro
#define SLLQueuePush_NZ(nil, f, l, n, next)                                    \
  (CheckNil(nil, f) ? ((f) = (l) = (n), SetNil(nil, (n)->next))                \
                    : ((l)->next = (n), (l) = (n), SetNil(nil, (n)->next)))
#define SLLQueuePush(f, l, n) SLLQueuePush_NZ(0, f, l, n, next)
// #define SLLQUeuePop(nil, f, l)

//- rjf: singly-linked, singly-headed lists (stacks)
#define SLLStackPush_N(f, n, next) ((n)->next = (f), (f) = (n))
#define SLLStackPop_N(f, next) ((f) = (f)->next)

//- rjf: singly-linked, singly-headed list helpers
#define SLLStackPush(f, n) SLLStackPush_N(f, n, next)
#define SLLStackPop(f) SLLStackPop_N(f, next)

// (NT): string types
// NOTE: for ease-of-use with C library that requires null terminated strings,
// the backing buffer should also accomodates for the
typedef struct {
  u8* str;
  u64 size;
} String8; // because each char is 8 bit

// string functions
// constructors
#define s(S) str8_lit(S)
#define str8_lit(s)                                                            \
  str8((u8*)(s), sizeof(s) - 1) // -1 because we don't need the null terminator
#define str8_const_lit(s)                                                            \
{(u8*)(s), sizeof(s) - 1}
#define str8_fmt(s) (int)((s).size), (s).str
#define StrEvalPrint(s) printf("%s = %.*s\n", #s, str8_fmt(s));
fn String8 str8(u8* str, u64 size);
fn String8 str8_cstring(char* str);
fn b32 char_is_space(u8 c); // copy from rjf
fn String8 str8_range(u8* first, u8* one_past_last);
fn String8 str8_substr(String8 str, Rng1U64);
fn String8 str8_concat(Arena* arena, String8 a, String8 b);
fn String8 str8_skip_chop_whitespace(String8 string); // copied from rjf
fn String8 str8_trim_whitespace(String8 a);
fn b32 str8_compare(String8 a, String8 b);
fn b32 str8_compare_cstring(String8 a, char* b);
fn u64 str8_find(String8 haystack,
                 String8 needle); // return the pos of the found string, ret
                                  // haystack size if not found

// string copy/formatting
#include <stdarg.h>
#include "third_party/stb_sprintf.h"
fn String8 str8_copy(Arena* arena, String8 str);
fn String8 str8fv(Arena* arena, char* fmt, va_list args);
fn String8 str8f(Arena* arena, char* fmt, ...);

// string conversion
fn i32 str8_to_i32(String8 str);
fn u64 str8_to_u64(String8 str);

// string list
typedef struct String8Node String8Node;
struct String8Node {
  String8Node* next;
  String8 string;
};

typedef struct String8List String8List;
struct String8List {
  String8Node* first;
  String8Node* last;
  u64 node_count;
  u64 total_size;
};

typedef struct StringJoin StringJoin;
struct StringJoin {
  String8 pre;
  String8 sep;
  String8 post;
};

fn String8Node* str8_list_push_node(String8List* list, String8Node* node);
fn String8Node* str8_list_push(Arena* arena, String8List* list, String8 string);
fn String8Node* str8_list_pushf(Arena* arena, String8List* list, char* fmt,
                                ...);
fn String8 str8_list_join(Arena* arena, String8List* list,
                          StringJoin* optional_params);
fn String8List str8_split(Arena* arena, String8 target, String8 delim);

////////////////////////////////
//~ rjf: Basic Text Indentation

fn String8 indented_from_string(Arena* arena, String8 string);

// logging system
// NOTE: copied from raddebugger project

//~ rjf: Log Types

typedef enum LogMsgKind {
  LogMsgKind_Info = 1 << 1,
  LogMsgKind_Warn = 1 << 2,
  LogMsgKind_UserError = 1 << 3,
  LogMsgKind_COUNT
} LogMsgKind;

typedef u8 LogMsgKindFlag;
#define LogMsgKind_All 0b11111111

typedef struct LogMsg LogMsg;
struct LogMsg {
  LogMsg* next;
  String8 string;
  LogMsgKind kind;
};

typedef struct LogScope LogScope;
struct LogScope {
  LogScope* next;
  u64 pos;
  // String8List strings[LogMsgKind_COUNT];
  LogMsg* first;
  LogMsg* last;
};

typedef struct LogScopeResult LogScopeResult;
struct LogScopeResult {
  // String8 strings[LogMsgKind_COUNT];
  String8 string;
};

typedef struct Log Log;
struct Log {
  Arena* arena;
  LogScope* top_scope;
};

////////////////////////////////
// Globals

extern String8 g_logs_folder;
extern __thread Log* log_active;

////////////////////////////////
//~ rjf: Log Creation/Selection

fn Log* log_alloc(void);
fn void log_release(Log* log);
fn void log_select(Log* log);

////////////////////////////////
//~ rjf: Log Building

fn void log_msg(LogMsgKind kind, String8 string);
fn void log_msgf(LogMsgKind kind, char* fmt, ...);
#define log_info(s) log_msg(LogMsgKind_Info, (s))
#define log_infof(...) log_msgf(LogMsgKind_Info, __VA_ARGS__)
#define log_user_error(s) log_msg(LogMsgKind_UserError, (s))
#define log_user_errorf(...) log_msgf(LogMsgKind_UserError, __VA_ARGS__)
#define log_warn(s) log_msg(LogMsgKind_Warn, (s))
#define log_warnf(...) log_msgf(LogMsgKind_Warn, __VA_ARGS__)

#define LogInfoNamedBlock(s)                                                   \
  DeferLoop(log_infof("%S:\n{\n", (s)), log_infof("}\n"))
#define LogInfoNamedBlockF(...)                                                \
  DeferLoop((log_infof(__VA_ARGS__), log_infof(":\n{\n")), log_infof("}\n"))

////////////////////////////////
//~ rjf: Log Scopes

fn void log_scope_begin(void);
fn LogScopeResult log_scope_end(Arena* arena, LogMsgKindFlag flag);
fn String8 log_scope_peek(Arena* arena, LogMsgKindFlag flag);
fn u64 log_scope_peek_count(LogMsgKindFlag flag);

// filesystem (linux only)
#include <stdio.h>

fn b32 write_data_to_file_path(String8 path, String8 data);
fn b32 append_data_to_file_path(String8 path, String8 data);

#endif // BASE_H
