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

#define EachIndex(it, count) (u64 it = 0; it < (count); it += 1) // 0..n-1
#define EachIndexRev(it, count) (u64 it = (count); it > 0; it -= 1) // n..1 (not n-1..0 because otherwise unsigned number cant use this)
#define EachNode(it, T, first) (T *it = first; it != 0; it = it->next)

#include <stdio.h>
#define EvalPrint(x) printf("%s = %d\n", #x, (int)(x))

// (NT): memory operation
#include <stdlib.h>
#include <string.h>
#define MemoryZero(x, sz) memset((x), 0, (sz));
#define Malloc(x) malloc(x);
#define Free(x) free(x);
#define AlignPow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))
#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a > b) ? b : a)

// (NT): arena allocator
typedef struct {
  u64 reserve_size;
  u64 commit_size;

  u64 pos;
  u64 commit_pos;
} Arena;

#define ARENA_BASE_POS (sizeof(Arena))
#define ARENA_ALIGN (sizeof(void *))

fn Arena *arena_alloc(u64 reserve_size, u64 commit_size);
fn void arena_release(Arena *arena);
fn void arena_set_auto_align(Arena *arena, u64 align);

fn u64 arena_pos(Arena *arena);

fn void *arena_push(Arena *arena, u64 size, b32 zero);
// fn void *arena_push_no_zero(Arena *arena, u64 size);
// fn void *arena_push_aligned(Arena *arena, u64 alignment);

fn void arena_pop(Arena *arena, u64 size);
fn void arena_pop_to(Arena *arena, u64 pos);
fn void arena_clear(Arena *arena);

// temp arena
// TODO: what for?
typedef struct {
  Arena *arena;
  u64 start_pos;
} Temp;

fn Temp temp_begin(Arena *arena);
fn void temp_end(Temp temp);

// scratch arena
fn Temp arena_scratch_get(Arena** conflicts, u32 num_conflicts);
fn void arena_scratch_release(Temp scratch);

fn u64 plat_get_pagesize(void);
fn void *plat_mem_reserve(u64 size);
fn b32 plat_mem_commit(void *ptr, u64 size);
fn b32 plat_mem_decommit(void *ptr, u64 size);
fn b32 plat_mem_release(void *ptr, u64 size);

// Nil check
#define CheckNil(nil, p) ((p) == 0 || (p) == (nil))
#define SetNil(nil, p) ((p) = (nil))

// Linked lists macro
#define SLLQueuePush_NZ(nil, f, l, n, next) (CheckNil(nil, f)?\
        ((f)=(l)=(n), SetNil(nil, (n)->next)):\
        ((l)->next = (n), (l)=(n), SetNil(nil, (n)->next)))
#define SLLQueuePush(f, l, n) SLLQueuePush_NZ(0, f, l, n, next)
// #define SLLQUeuePop(nil, f, l) 

// (NT): string types
// NOTE: for ease-of-use with C library that requires null terminated strings, the backing buffer should also accomodates for the 
typedef struct {
  u8 *str;
  u64 size;
} String8; // because each char is 8 bit

// string functions
fn String8 str8(u8 *str, u64 size);
fn String8 str8_cstring(char* str);
fn String8 str8_range(u8 *first, u8 *one_past_last);
fn String8 str8_substr(String8 *str, u64 start, u64 end);
fn String8 str8_concat(Arena *arena, String8 a, String8 b);
fn String8 str8_trim_whitespace(String8 *a);
fn b32 str8_compare(String8 *a, String8 *b);
fn b32 str8_compare_cstring(String8 *a, char *b);

// string copy/formatting
#include <stdarg.h>
#include "third_party/stb_sprintf.h"
fn String8 str8_copy(Arena* arena, String8 str);
fn String8 str8fv(Arena *arena, char *fmt, va_list args);
fn String8 str8f(Arena *arena, char *fmt, ...);

// string conversion
fn i32 str8_to_i32(String8* str);

typedef struct String8Node String8Node;
struct String8Node {
  String8Node *next;
  String8 str;
};

typedef struct String8List String8List;
struct String8List {
  String8Node *first;
  String8Node *last;
  u64 node_count;
  u64 total_size;
};

fn String8Node *str8_list_push_node(String8List *list, String8Node *node);
fn String8List str8_split(Arena *arena, String8 *target, String8 *delim);

#define str8_lit(s)                                                            \
  str8((u8 *)(s), sizeof(s) - 1) // -1 because we don't need the null terminator
#define str8_fmt(s) (int)((s).size), (s).str
#define StrEvalPrint(s) printf("%s = %.*s\n", #s, str8_fmt(s));

#endif // BASE_H
