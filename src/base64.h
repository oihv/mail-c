#ifndef BASE64_H
#define BASE64_H

static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String8 base64_encode(Arena* arena, String8 src);
String8 base64_decode(Arena* arena, String8 src);

#endif // BASE64_H


