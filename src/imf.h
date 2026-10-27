#ifndef IMF_H
#define IMF_H

#include <time.h>

fn String8 imf_local_date(Arena* arena);
fn String8 imf_message_compose(Arena* arena, String8 date, String8 from,
                               String8 data);

#endif // IMF_H
