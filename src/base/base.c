#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

fn Arena *arena_alloc(u64 reserve_size, u64 commit_size) {
  u32 pagesize = plat_get_pagesize();

  reserve_size = AlignPow2(reserve_size, pagesize);
  commit_size = AlignPow2(commit_size, pagesize);

  Arena *arena =
      plat_mem_reserve(reserve_size); // store the pointer to the arena in the
                                      // first chunk of memory

  if (!plat_mem_commit(arena, commit_size)) {
    return NULL;
  }

  arena->reserve_size = reserve_size;
  arena->commit_size = commit_size;
  arena->pos = ARENA_BASE_POS;
  arena->commit_size = commit_size;

  return arena;
}

fn void arena_release(Arena *arena) {
  plat_mem_release(arena, arena->reserve_size);
}

fn u64 arena_pos(Arena *arena) { return arena->pos; }

// fn void *arena_push_no_zero(Arena *arena, u64 size);
// fn void *arena_push_aligned(Arena *arena, u64 alignment);

fn void *arena_push(Arena *arena, u64 size, b32 zero) {
  u64 pos_aligned = AlignPow2(arena->pos, ARENA_ALIGN);
  u64 new_pos = pos_aligned + size;

  if (new_pos > arena->reserve_size) {
    return NULL;
  }

  if (new_pos > arena->commit_size) {
    // round up to the next multiple of commit_size
    u64 new_commit_pos = new_pos;
    // if we didn't minus 1, it will round up even if its perfectly aligned
    new_commit_pos += arena->commit_size - 1;
    new_commit_pos -= new_commit_pos % arena->commit_size;
    new_commit_pos = MIN(new_commit_pos, arena->reserve_size);

    // cast to u8* so the pointer arithmetic is done by 1 byte
    u8 *mem = (u8 *)arena + arena->commit_pos;
    u64 commit_size = new_commit_pos - arena->commit_pos;

    if (!plat_mem_commit(mem, commit_size)) {
      return NULL;
    }

    arena->commit_pos = new_commit_pos;
  }
  arena->pos = new_pos;

  u8 *out = (u8 *)arena + pos_aligned;

  if (zero) {
    memset(out, 0, size);
  }

  return out;
}

fn void arena_pop(Arena *arena, u64 size) {
  size = MIN(size, arena->pos - ARENA_BASE_POS);
  arena->pos -= size;
}

fn void arena_pop_to(Arena *arena, u64 pos) {
  u64 size = pos < arena->pos ? arena->pos - pos : 0;
  arena_pop(arena, size);
}

fn void arena_clear(Arena *arena) { arena_pop_to(arena, ARENA_BASE_POS); }

// scratch arena
fn Temp temp_begin(Arena *arena) {
  return (Temp){.arena = arena, .start_pos = arena->pos};
}

fn void temp_end(Temp temp) { arena_pop_to(temp.arena, temp.start_pos); }

static __thread Arena *_scratch_arenas[2] = {NULL, NULL};

fn Temp arena_scratch_get(Arena **conflicts, u32 num_conflicts) {
  i32 scratch_index = -1;

  for (i32 i = 0; i < 2; i++) {
    b32 conflict_found = false;

    for (u32 j = 0; j < num_conflicts; j++) {
      if (_scratch_arenas[i] == conflicts[j]) {
        conflict_found = true;
        break;
      }
    }

    if (!conflict_found) {
      scratch_index = i;
      break;
    }
  }

  if (scratch_index == -1) {
    return (Temp){0};
  }

  Arena **selected = &_scratch_arenas[scratch_index];

  if (*selected == NULL) {
    *selected = arena_alloc(MB(64), MB(1));
  }

  return temp_begin(*selected);
}

fn void arena_scratch_release(Temp scratch) { temp_end(scratch); }

// platform virtual memory function wrappers
#if defined(__linux__)

#ifndef _DEFAULT_SOURCE // what for??
#define _DEFAULT_SOURCE
#endif

#include <sys/mman.h>
#include <unistd.h>

// TODO: understand by heart what does this all means..
u64 plat_get_pagesize(void) { return (u64)getpagesize(); }

void *plat_mem_reserve(u64 size) {
  void *out = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (out == MAP_FAILED) {
    return NULL;
  }
  return out;
}

b32 plat_mem_commit(void *ptr, u64 size) {
  i32 ret = mprotect(ptr, size, PROT_READ | PROT_WRITE);
  return ret == 0;
}

b32 plat_mem_decommit(void *ptr, u64 size) {
  i32 ret = mprotect(ptr, size, PROT_NONE);
  if (ret != 0)
    return false;
  ret = madvise(ptr, size, MADV_DONTNEED);
  return ret == 0;
}

b32 plat_mem_release(void *ptr, u64 size) {
  i32 ret = munmap(ptr, size);
  return ret == 0;
}
#endif
// strings
fn String8 str8(u8 *str, u64 size) {
  String8 result = {str, size};
  return result;
}

fn String8 str8_cstring(char *str) {
  String8 result = {(u8 *)str, strlen(str)};
  return result;
}

fn String8 str8_range(u8 *first, u8 *one_past_last) {
  String8 result = {first, (u64)(one_past_last - first)};
  return result;
}

fn String8 str8_substr(String8 str, u64 start, u64 end) {
  String8 result = str8_range(str.str + start, str.str + end);
  return result;
}

fn String8 str8_concat(Arena *arena, String8 a, String8 b) {
  String8 result = {.size = a.size + b.size};
  result.str = arena_push(arena, result.size, 1);
  for
    EachIndex(i, a.size) { result.str[i] = a.str[i]; }
  for
    EachIndex(i, b.size) { result.str[i + a.size] = b.str[i]; }
  return result;
}

fn String8 str8_trim_whitespace(String8 a) {
  u8 *start = 0;
  u8 *end = 0;
  for
    EachIndex(i, a.size) {
      if (a.str[i] != ' ') {
        start = a.str + i;
        break;
      }
    }
  for
    EachIndexRev(i, a.size) {
      if (a.str[i - 1] != ' ') {
        end = a.str + i; // one pass last
        break;
      }
    }
  String8 result = str8_range(start, end);
  return result;
}

fn b32 str8_compare(String8 a, String8 b) {
  if (a.size != b.size)
    return false;
  for
    EachIndex(i, a.size) {
      if (a.str[i] != b.str[i])
        return false;
    }
  return true;
}

fn b32 str8_compare_cstring(String8 a, char *b) {
  String8 b_str = str8_cstring(b);
  return str8_compare(a, b_str);
}

fn u64 str8_find(String8 haystack, String8 needle) {
  u64 pos = haystack.size;
  for (u8* p = haystack.str; p + needle.size <= haystack.str + haystack.size; p++) {
    String8 sub = str8_range(p, p + needle.size);
    if (str8_compare(sub, needle)) {
      pos = p - haystack.str;
      break;
    }
  }

  return pos;
}

// string copy/formatting
fn String8 str8_copy(Arena *arena, String8 s) {
  String8 str;
  str.size = s.size;
  str.str = arena_push(arena, str.size + 1, 1);
  memcpy(str.str, s.str, s.size);
  str.str[str.size] = 0;
  return str;
}

fn String8 str8fv(Arena *arena, char *fmt, va_list args) {
  va_list args2;
  va_copy(args2, args);
  u32 needed_bytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
  String8 result = {0};
  result.str = arena_push(arena, needed_bytes * sizeof(u8), 0);
  result.size = stbsp_vsnprintf((char *)result.str, needed_bytes, fmt, args2);
  result.str[result.size] = 0;
  va_end(args2);
  return result;
}

fn String8 str8f(Arena *arena, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  String8 result = str8fv(arena, fmt, args);
  va_end(args);
  return result;
}

// string conversion
fn i32 str8_to_i32(String8 str) {
  i32 acc = 0;
  i32 mul = 1;
  for
    EachIndexRev(i, str.size) {
      acc += (str.str[i - 1] - '0') * mul;
      mul *= 10;
    }
  return acc;
}

fn String8Node *str8_list_push_node(String8List *list, String8Node *node) {
  SLLQueuePush(list->first, list->last, node);
  list->node_count += 1;
  list->total_size += node->str.size;
  return node;
}

fn String8List str8_split(Arena *arena, String8 target, String8 delim) {
  u64 prev = 0;
  String8List result = {0};
  for (int i = 0; i + delim.size <= target.size; i++) {
    String8 substr = str8_range(target.str + i, target.str + i + delim.size);
    if (str8_compare(substr, delim)) {
      String8Node *node = arena_push(arena, sizeof(String8Node), 1);
      node->str.str = target.str + prev;
      node->str.size = i - prev;
      str8_list_push_node(&result, node);
      i += delim.size;
      prev = i;
    }
  }
  // add last split
  if (prev < target.size) {
    String8Node *node = arena_push(arena, sizeof(String8Node), 1);
    node->str.str = target.str + prev;
    node->str.size = target.size - prev;
    str8_list_push_node(&result, node);
  }
  return result;
}
