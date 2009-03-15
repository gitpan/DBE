#ifndef __CHARSET_H__
#define __CHARSET_H__ 1

#include "common.h"

enum n_charset {
	CS_UNKNOWN = -1,
	CS_ANSI = 0,
	CS_UTF8,
#define CS_UTF16 CS_UCS2BE
	CS_UCS2BE,
	CS_UCS2LE
};

#define CS_CNV_ERROR (size_t) -1

EXTERN const char *get_charset_name( enum n_charset cs );
EXTERN enum n_charset get_charset_id( const char *name );
EXTERN size_t charset_convert(
	char *src, size_t src_len, enum n_charset cs_src, enum n_charset cs_dst,
	char **p_dst
);
EXTERN SV *charset_quote( enum n_charset cs, const char *str, size_t str_len );
EXTERN SV *charset_quote_id( enum n_charset cs, const char **args, int argc );

EXTERN size_t utf8_strlen( const char *str );

EXTERN size_t ansi_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ansi_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ansi_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);

size_t utf8_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t utf8_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t utf8_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);

size_t ucs2le_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ucs2le_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ucs2le_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);

size_t ucs2be_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ucs2be_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);
size_t ucs2be_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
);

#endif
