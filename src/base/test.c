#include "base.h"
#include "base.c"

typedef enum {
  Flag_anjay = (1 << 0),
  Flag_siap = (1 << 1),
  Flag_coba = (1 << 2),
} Example_Flag;
int main() {
  Arena* perm_arena = arena_alloc(MB(5), KB(4));

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
    String8 sub1 = str8_substr(test, r1u64(0, 4));
    String8 sub2 = str8_substr(test, r1u64(5, 12));
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
    for
      EachNode(i, String8Node, res1.first) { StrEvalPrint(i->string); }
    printf("---------------single chars2:\n");
    String8 test3 = str8_lit("op1=12op2=23op3=34");
    StrEvalPrint(test3);
    String8 delim3 = str8_lit("");
    StrEvalPrint(delim3);
    String8List res3 = str8_split(perm_arena, test3, delim3);
    for
      EachNode(i, String8Node, res3.first) { StrEvalPrint(i->string); }
    printf("---------------single chars3:\n");
    String8 test4 = str8_lit("op1=34");
    StrEvalPrint(test4);
    String8 delim4 = str8_lit("=");
    StrEvalPrint(delim4);
    String8List res4 = str8_split(perm_arena, test4, delim4);
    for
      EachNode(i, String8Node, res4.first) { StrEvalPrint(i->string); }
    printf("---------------single chars4:\n");
    String8 test5 = str8_lit("<<nikolas@bit.com>");
    StrEvalPrint(test5);
    String8 delim5 = str8_lit("<");
    StrEvalPrint(delim5);
    String8List res5 = str8_split(perm_arena, test5, delim5);
    for
      EachNode(i, String8Node, res5.first) { StrEvalPrint(i->string); }
    printf("---------------multi chars:\n");
    String8 test2 =
        str8_lit("nikolasanjaycprogramminganjayarenaallocatoranjaysimplicity");
    StrEvalPrint(test2);
    String8 delim2 = str8_lit("anjay");
    StrEvalPrint(delim2);
    String8List res2 = str8_split(perm_arena, test2, delim2);
    for
      EachNode(i, String8Node, res2.first) { StrEvalPrint(i->string); }
    printf("\n");
  }

  {
    printf("----------Join string list:\n");
    {
      printf("---------------no optional param:\n");
      String8List list1 = {0};
      String8Node node1 = {.string = str8_lit("log system")};
      String8Node node2 = {.string = str8_lit("pop3")};
      String8Node node3 = {.string = str8_lit("smtp")};
      String8Node node4 = {.string = str8_lit("gui")};
      String8Node node5 = {.string = str8_lit("database")};
      str8_list_push_node(&list1, &node1);
      str8_list_push_node(&list1, &node2);
      str8_list_push_node(&list1, &node3);
      str8_list_push_node(&list1, &node4);
      str8_list_push_node(&list1, &node5);
      for
        EachNode(i, String8Node, list1.first) { StrEvalPrint(i->string); }
      String8 res = str8_list_join(perm_arena, &list1, 0);
      StrEvalPrint(res);
      printf("\n");
    }
    {
      printf("---------------with optional param:\n");
      String8List list1 = {0};
      StringJoin join = {.pre = str8_lit("network project:"), .sep = str8_lit("|"), .post = str8_lit("done")};;
      StrEvalPrint(join.pre);
      StrEvalPrint(join.sep);
      StrEvalPrint(join.post);
      str8_list_push(perm_arena, &list1, str8_lit("log system"));
      str8_list_push(perm_arena, &list1, str8_lit("pop3"));
      str8_list_push(perm_arena, &list1, str8_lit("smtp"));
      str8_list_push(perm_arena, &list1, str8_lit("gui"));
      str8_list_push(perm_arena, &list1, str8_lit("database"));
      str8_list_push(perm_arena, &list1, str8_lit(".\r\n"));
      for
        EachNode(i, String8Node, list1.first) { StrEvalPrint(i->string); }
      String8 res = str8_list_join(perm_arena, &list1, &join);
      StrEvalPrint(res);
      printf("\n");
    }
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

  {
    printf("----------write file:\n");
    String8 test = str8_lit("here is a test chunk");
    StrEvalPrint(test);
    // write_data_to_file_path(str8_lit("./test.txt"), test);
    printf("\n");
  }
  {
    printf("----------append file:\n");
    String8 test = str8_lit("newly added chunk");
    StrEvalPrint(test);
    // append_data_to_file_path(str8_lit("./test.txt"), test);
    printf("\n");
  }

  arena_release(perm_arena);

  return 0;
}
