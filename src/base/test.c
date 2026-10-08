#include "base.h"
#include "base.c"

typedef enum {
  Flag_anjay = (1 << 0),
  Flag_siap = (1 << 1),
  Flag_coba = (1 << 2),
} Example_Flag;
int main() {
  Arena *perm_arena = arena_alloc(MB(5), KB(4));

  {
    printf("-----------String concat:\n");
    String8 str1 = str8_lit("anjay");
    String8 str2 = str8_lit("mabar");
    StrEvalPrint(str1);
    StrEvalPrint(str2);
    String8 res = str8_concat(perm_arena, str1, str2);
    StrEvalPrint(res);
    printf("\n");
  }

  {
    printf("----------Trim whitespace:\n");
    String8 str_with_whitespace = str8_lit("     Steve Jobs     ");
    String8 str_without_whitespace = str8_trim_whitespace(str_with_whitespace);
    StrEvalPrint(str_with_whitespace);
    StrEvalPrint(str_without_whitespace);
    printf("\n");
  }

  {
    printf("----------Compare strings:\n");
    {
      String8 str1 = str8_lit("anjay");
      String8 str2 = str8_lit("anjay");
      StrEvalPrint(str1);
      StrEvalPrint(str2);
      b32 res = str8_compare(str1, str2);
      EvalPrint(res);
    }
    {
      String8 str1 = str8_lit("anjay");
      String8 str2 = str8_lit("anjayx");
      StrEvalPrint(str1);
      StrEvalPrint(str2);
      b32 res = str8_compare(str1, str2);
      EvalPrint(res);
    }
    {
      String8 str1 = str8_lit("anjay");
      String8 str2 = str8_lit("xanjay");
      StrEvalPrint(str1);
      StrEvalPrint(str2);
      b32 res = str8_compare(str1, str2);
      EvalPrint(res);
    }
    {
      String8 str1 = str8_lit("anjay");
      String8 str2 = str8_lit("bukan");
      StrEvalPrint(str1);
      StrEvalPrint(str2);
      b32 res = str8_compare(str1, str2);
      EvalPrint(res);
    }
    printf("\n");
  }

  {
    printf("----------Compare strings:\n");
    String8 test = str8_lit("John Carmack");
    String8 sub1 = str8_substr(test, 0, 4);
    String8 sub2 = str8_substr(test, 5, 12);
    StrEvalPrint(sub1);
    StrEvalPrint(sub2);
    printf("\n");
  }

  {
    printf("----------String split:\n");
    printf("---------------single chars1:\n");
    String8 test1 = str8_lit("anjay mabar siap bos");
    StrEvalPrint(test1);
    String8 delim1 = str8_lit(" ");
    StrEvalPrint(delim1);
    String8List res1 = str8_split(perm_arena, test1, delim1);
    for EachNode(i, String8Node, res1.first) {
      StrEvalPrint(i->str);
    }
    printf("---------------single chars2:\n");
    String8 test3 = str8_lit("op1=12op2=23op3=34");
    StrEvalPrint(test3);
    String8 delim3 = str8_lit("");
    StrEvalPrint(delim3);
    String8List res3 = str8_split(perm_arena, test3, delim3);
    for EachNode(i, String8Node, res3.first) {
      StrEvalPrint(i->str);
    }
    printf("---------------single chars:\n");
    String8 test4 = str8_lit("op1=34");
    StrEvalPrint(test4);
    String8 delim4 = str8_lit("=");
    StrEvalPrint(delim4);
    String8List res4 = str8_split(perm_arena, test4, delim4);
    for EachNode(i, String8Node, res4.first) {
      StrEvalPrint(i->str);
    }
    printf("---------------multi chars:\n");
    String8 test2 = str8_lit("nikolasanjaycprogramminganjayarenaallocatoranjaysimplicity");
    StrEvalPrint(test2);
    String8 delim2 = str8_lit("anjay");
    StrEvalPrint(delim2);
    String8List res2 = str8_split(perm_arena, test2, delim2);
    for EachNode(i, String8Node, res2.first) {
      StrEvalPrint(i->str);
    }
    printf("\n");
  }

  {
    printf("----------Find string:\n");
    String8 test1 = str8_lit("ilovepokemongo");
    String8 target1 = str8_lit("pokemon");
    u64 pos1 = str8_find(test1, target1);
    StrEvalPrint(test1);
    StrEvalPrint(target1);
    EvalPrint(pos1);
    String8 test2 = str8_lit("SupremeCalamitas");
    String8 target2 = str8_lit("Cal");
    u64 pos2 = str8_find(test2, target2);
    StrEvalPrint(test2);
    StrEvalPrint(target2);
    EvalPrint(pos2);
    printf("\n");
  }

  arena_release(perm_arena);

  return 0;
}
