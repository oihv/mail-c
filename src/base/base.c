#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

// math
fn Rng1U64 r1u64(u64 min, u64 max) {
  Rng1U64 r = {{min, max}};
  if (r.min > r.max) {
    Swap(u64, r.min, r.max);
  }
  return r;
}

// arena
fn Arena* arena_alloc(u64 reserve_size, u64 commit_size) {
  u32 pagesize = plat_get_pagesize();

  reserve_size = AlignPow2(reserve_size, pagesize);
  commit_size = AlignPow2(commit_size, pagesize);

  Arena* arena =
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

fn void arena_release(Arena* arena) {
  plat_mem_release(arena, arena->reserve_size);
}

fn u64 arena_pos(Arena* arena) { return arena->pos; }

// fn void *arena_push_no_zero(Arena *arena, u64 size);
// fn void *arena_push_aligned(Arena *arena, u64 alignment);

fn void* arena_push(Arena* arena, u64 size, b32 zero) {
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
    new_commit_pos = Min(new_commit_pos, arena->reserve_size);

    // cast to u8* so the pointer arithmetic is done by 1 byte
    u8* mem = (u8*)arena + arena->commit_pos;
    u64 commit_size = new_commit_pos - arena->commit_pos;

    if (!plat_mem_commit(mem, commit_size)) {
      return NULL;
    }

    arena->commit_pos = new_commit_pos;
  }
  arena->pos = new_pos;

  u8* out = (u8*)arena + pos_aligned;

  if (zero) {
    memset(out, 0, size);
  }

  return out;
}

fn void arena_pop(Arena* arena, u64 size) {
  size = Min(size, arena->pos - ARENA_BASE_POS);
  arena->pos -= size;
}

fn void arena_pop_to(Arena* arena, u64 pos) {
  u64 size = pos < arena->pos ? arena->pos - pos : 0;
  arena_pop(arena, size);
}

fn void arena_clear(Arena* arena) { arena_pop_to(arena, ARENA_BASE_POS); }

// scratch arena
fn Temp temp_begin(Arena* arena) {
  return (Temp){.arena = arena, .start_pos = arena->pos};
}

fn void temp_end(Temp temp) { arena_pop_to(temp.arena, temp.start_pos); }

static __thread Arena* _scratch_arenas[2] = {NULL, NULL};

fn Temp arena_scratch_get(Arena** conflicts, u32 num_conflicts) {
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

  Arena** selected = &_scratch_arenas[scratch_index];

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

void* plat_mem_reserve(u64 size) {
  void* out = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (out == MAP_FAILED) {
    return NULL;
  }
  return out;
}

b32 plat_mem_commit(void* ptr, u64 size) {
  i32 ret = mprotect(ptr, size, PROT_READ | PROT_WRITE);
  return ret == 0;
}

b32 plat_mem_decommit(void* ptr, u64 size) {
  i32 ret = mprotect(ptr, size, PROT_NONE);
  if (ret != 0)
    return false;
  ret = madvise(ptr, size, MADV_DONTNEED);
  return ret == 0;
}

b32 plat_mem_release(void* ptr, u64 size) {
  i32 ret = munmap(ptr, size);
  return ret == 0;
}
#endif

// strings
fn String8 str8(u8* str, u64 size) {
  String8 result = {str, size};
  return result;
}

fn String8 str8_cstring(char* str) {
  String8 result = {(u8*)str, strlen(str)};
  return result;
}

// string classification
fn b32 char_is_space(u8 c) {
  return (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' ||
          c == '\v');
}

fn String8 str8_range(u8* first, u8* one_past_last) {
  String8 result = {first, (u64)(one_past_last - first)};
  return result;
}

fn String8 str8_substr(String8 str, Rng1U64 range) {
  String8 result = str8_range(str.str + range.min, str.str + range.max);
  return result;
}

fn String8 str8_concat(Arena* arena, String8 a, String8 b) {
  String8 result = {.size = a.size + b.size};
  result.str = arena_push(arena, result.size * sizeof(u8), 1);
  ForEachIndex(i, a.size) { result.str[i] = a.str[i]; }
  ForEachIndex(i, b.size) { result.str[i + a.size] = b.str[i]; }
  return result;
}

fn String8 str8_skip_chop_whitespace(String8 string) {
  u8* first = string.str;
  u8* opl = first + string.size; // one pass last
  for (; first < opl; first += 1) {
    if (!char_is_space(*first)) {
      break;
    }
  }
  for (; opl > first;) {
    opl -= 1;
    if (!char_is_space(*opl)) {
      opl += 1;
      break;
    }
  }
  String8 result = str8_range(first, opl);
  return result;
}

// obsolete in favor of opposed to str8_skip_chop_whitespace
fn String8 str8_trim_whitespace(String8 a) {
  u8* start = 0;
  u8* end = 0;
  ForEachIndex(i, a.size) {
    if (!char_is_space(a.str[i])) {
      start = a.str + i;
      break;
    }
  }
  ForEachIndexRev(i, a.size) {
    if (!char_is_space(a.str[i])) {
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
  ForEachIndex(i, a.size) {
    if (a.str[i] != b.str[i])
      return false;
  }
  return true;
}

fn b32 str8_compare_cstring(String8 a, char* b) {
  String8 b_str = str8_cstring(b);
  return str8_compare(a, b_str);
}

fn u64 str8_find(String8 haystack, String8 needle) {
  u64 pos = haystack.size;
  for (u8* p = haystack.str; p + needle.size <= haystack.str + haystack.size;
       p++) {
    String8 sub = str8_range(p, p + needle.size);
    if (str8_compare(sub, needle)) {
      pos = p - haystack.str;
      break;
    }
  }

  return pos;
}

// string copy/formatting
fn String8 str8_copy(Arena* arena, String8 s) {
  String8 str;
  str.size = s.size;
  str.str = arena_push(arena, (str.size + 1) * sizeof(u8), 1);
  memcpy(str.str, s.str, s.size);
  str.str[str.size] = 0;
  return str;
}

fn String8 str8fv(Arena* arena, char* fmt, va_list args) {
  va_list args2;
  va_copy(args2, args);
  u32 needed_bytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
  String8 result = {0};
  result.str = arena_push(arena, needed_bytes * sizeof(u8), 0);
  result.size = stbsp_vsnprintf((char*)result.str, needed_bytes, fmt, args2);
  result.str[result.size] = 0;
  va_end(args2);
  return result;
}

fn String8 str8f(Arena* arena, char* fmt, ...) {
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
  ForEachIndexRev(i, str.size) {
    acc += (str.str[i - 1] - '0') * mul;
    mul *= 10;
  }
  return acc;
}

fn u64 str8_to_u64(String8 str) {
  u64 acc = 0;
  u64 mul = 1;
  ForEachIndexRev(i, str.size) {
    acc += (str.str[i - 1] - '0') * mul;
    mul *= 10;
  }
  return acc;
}

fn String8Node* str8_list_push_node(String8List* list, String8Node* node) {
  SLLQueuePush(list->first, list->last, node);
  list->node_count += 1;
  list->total_size += node->string.size;
  return node;
}

fn String8Node* str8_list_push(Arena* arena, String8List* list,
                               String8 string) {
  String8Node* node = arena_push(arena, sizeof(String8Node), 1);
  node->string = string;
  str8_list_push_node(list, node);
  return node;
}

fn String8Node* str8_list_pushf(Arena* arena, String8List* list, char* fmt,
                                ...) {
  va_list args;
  va_start(args, fmt);
  String8 string = str8fv(arena, fmt, args);
  String8Node* result = str8_list_push(arena, list, string);
  va_end(args);
  return result;
}

fn String8 str8_list_join(Arena* arena, String8List* list,
                          StringJoin* optional_params) {
  StringJoin join = {0};
  if (optional_params != 0) {
    memcpy(&join, optional_params, sizeof(join));
  }
  u64 sep_count = 0;
  if (list->node_count > 0) {
    sep_count = list->node_count - 1;
  }
  String8 res = {0};
  res.size = join.pre.size + sep_count * join.sep.size + list->total_size +
             join.post.size;
  u8* ptr = res.str = (u8*)arena_push(arena, res.size * sizeof(u8), 1);
  memcpy(ptr, join.pre.str, join.pre.size);
  ptr += join.pre.size;
  ForEachNode(node, String8Node, list->first) {
    memcpy(ptr, node->string.str, node->string.size);
    ptr += node->string.size;
    if (node->next != 0) {
      memcpy(ptr, join.sep.str, join.sep.size);
      ptr += join.sep.size;
    }
  }
  memcpy(ptr, join.post.str, join.post.size);
  ptr += join.post.size;
  *ptr = 0;
  return res;
}

fn String8List str8_split(Arena* arena, String8 target, String8 delim) {
  u64 prev = 0;
  String8List result = {0};
  for (int i = 0; i + delim.size <= target.size; i++) {
    String8 substr = str8_range(target.str + i, target.str + i + delim.size);
    if (str8_compare(substr, delim)) {
      String8Node* node = arena_push(arena, sizeof(String8Node), 1);
      node->string.str = target.str + prev;
      node->string.size = i - prev;
      str8_list_push_node(&result, node);
      i += delim.size;
      prev = i;
      if (delim.size > 0)
        i--;
    }
  }
  // add last split
  if (prev < target.size) {
    String8Node* node = arena_push(arena, sizeof(String8Node), 1);
    node->string.str = target.str + prev;
    node->string.size = target.size - prev;
    str8_list_push_node(&result, node);
  }
  return result;
}

////////////////////////////////
//~ rjf: Basic Text Indentation

fn String8 indented_from_string(Arena* arena, String8 string) {
  Temp scratch = arena_scratch_get(&arena, 1);
  static u8 indentation_bytes[] =
      "                                                                        "
      "                                                        ";
  String8List indented_strings = {0};
  i64 depth = 0;
  i64 next_depth = 0;
  u64 line_begin_off = 0;
  for (u64 off = 0; off <= string.size; off += 1) {
    u8 byte = off < string.size ? string.str[off] : 0;
    switch (byte) {
    default: {
    } break;
    case '{':
    case '[':
    case '(': {
      next_depth += 1;
      next_depth = Max(0, next_depth);
    } break;
    case '}':
    case ']':
    case ')': {
      next_depth -= 1;
      next_depth = Max(0, next_depth);
      depth = next_depth;
    } break;
    case '\n':
    case 0: {
      String8 line = str8_skip_chop_whitespace(
          str8_substr(string, r1u64(line_begin_off, off)));
      if (line.size != 0) {
        str8_list_pushf(scratch.arena, &indented_strings, "%.*s%.*s\n",
                        (int)depth * 2, indentation_bytes, str8_fmt(line));
      }
      if (line.size == 0 && indented_strings.node_count != 0 &&
          off < string.size) {
        str8_list_pushf(scratch.arena, &indented_strings, "\n");
      }
      line_begin_off = off + 1;
      depth = next_depth;
    } break;
    }
  }
  String8 result = str8_list_join(arena, &indented_strings, 0);
  arena_scratch_release(scratch);
  return result;
}

// copied from raddebugger project
__thread Log* log_active;
String8 g_logs_folder;

////////////////////////////////
//~ rjf: Log Creation/Selection

fn Log* log_alloc(void) {
  Arena* arena = arena_alloc(MB(64), KB(64));
  Log* log = arena_push(arena, sizeof(Log), 1);
  log->arena = arena;
  return log;
}

fn void log_release(Log* log) { arena_release(log->arena); }

fn void log_select(Log* log) { log_active = log; }

////////////////////////////////
//~ rjf: Log Building/Clearing

fn void log_msg(LogMsgKind kind, String8 string) {
  if (log_active != 0 && log_active->top_scope != 0) {
    String8 string_copy = str8_copy(log_active->arena, string);
    // str8_list_push(log_active->arena, &log_active->top_scope->strings[kind],
    // string_copy);
    LogMsg* msg = arena_push(log_active->arena, sizeof(LogMsg), 1);
    msg->kind = kind;
    msg->string = string_copy;
    SLLQueuePush(log_active->top_scope->first, log_active->top_scope->last,
                 msg);
  }
}

fn void log_msgf(LogMsgKind kind, char* fmt, ...) {
  if (log_active != 0) {
    Temp scratch = arena_scratch_get(0, 0);
    va_list args;
    va_start(args, fmt);
    String8 string = str8fv(scratch.arena, fmt, args);
    log_msg(kind, string);
    va_end(args);
    arena_scratch_release(scratch);
  }
}

////////////////////////////////
//~ rjf: Log Scopes

fn void log_scope_begin(void) {
  if (log_active != 0) {
    u64 pos = arena_pos(log_active->arena);
    LogScope* scope = arena_push(log_active->arena, sizeof(LogScope), 1);
    scope->pos = pos;
    SLLStackPush(log_active->top_scope, scope);
  }
}

fn LogScopeResult log_scope_end(Arena* arena, LogMsgKindFlag flag) {
  LogScopeResult result = {0};
  if (log_active != 0) {
    LogScope* scope = log_active->top_scope;
    if (scope != 0) {
      if (arena != 0) {
        // ForEachEnumVal(LogMsgKind, kind) {
        //   Temp scratch = arena_scratch_get(&arena, 1);
        //   String8 result_unindented =
        //       str8_list_join(scratch.arena, &scope->strings[kind], 0);
        //   result.strings[kind] = indented_from_string(arena,
        //   result_unindented); arena_scratch_release(scratch);
        // }

        result.string = log_scope_peek(arena, flag);
      }
      SLLStackPop(log_active->top_scope);
      arena_pop_to(log_active->arena, scope->pos);
    }
  }
  return result;
}

const String8 _level_prefixes[] = {
    [LogMsgKind_Info] = str8_const_lit("[INFO] "),
    [LogMsgKind_Warn] = str8_const_lit("[WARN] "),
    [LogMsgKind_UserError] = str8_const_lit("[ERROR] "),
    str8_const_lit("Unknown Log: "),
};

fn String8 log_scope_peek(Arena* arena, LogMsgKindFlag flag) {
  String8 result = {0};
  if (log_active != 0) {
    LogScope* scope = log_active->top_scope;
    if (scope != 0) {
      Temp scratch = arena_scratch_get(&arena, 1);
      String8List result_list = {0};
      ForEachNode(msg, LogMsg, scope->first) {
        if (msg->kind & flag) {
          String8 prefix = _level_prefixes[msg->kind];
          String8 msg_prefixed =
              str8_concat(scratch.arena, prefix, msg->string);
          str8_list_push(scratch.arena, &result_list, msg_prefixed);
        }
        result = str8_list_join(arena, &result_list,
                                &(StringJoin){.sep = str8_lit("\n")});
      }
      // result = scope->strings[kind];
    }
  }
  return result;
}

fn u64 log_scope_peek_count(LogMsgKindFlag flag) {
  u64 result = 0;

  if (log_active != 0) {
    LogScope* scope = log_active->top_scope;
    if (scope != 0) {
      ForEachNode(msg, LogMsg, scope->first) {
        if (flag & msg->kind) {
          result += 1;
        }
      }
    }
  }
  return result;
}

// file system
#include <fcntl.h>
fn b32 write_data_to_file_path(String8 path, String8 data) {
  b32 good = 0;
  int file = open((char*)path.str, O_WRONLY, 0755);
  if (file != 0) {
    u64 bytes_written = pwrite(file, (char*)data.str, data.size, 0);
    good = (bytes_written == data.size);
    close(file);
  }
  return good;
}

#include <sys/stat.h>
fn b32 append_data_to_file_path(String8 path, String8 data) {
  b32 good = 0;
  if (data.size != 0) {
    int file = open((char*)path.str, O_WRONLY | O_APPEND | O_CREAT, 0755);
    if (file != 0) {
      struct stat fd_stat = {0};
      stat((char*)data.str, &fd_stat);
      u64 pos = fd_stat.st_size;
      u64 bytes_written = pwrite(file, (char*)data.str, pos + data.size, pos);
      good = (bytes_written == data.size);
      close(file);
    }
  }
  return good;
}
