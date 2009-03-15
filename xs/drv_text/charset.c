#include "charset.h"

size_t ansi_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	byte *sp = src, *se = src + src_len;
	if( src == NULL )
		return 0;
	dst_len --;
	if( dst != NULL ) {
		byte *dp = dst, *de = dst + dst_len;
		for( ; sp < se && dp < de; sp ++ ) {
			*dp ++ = (byte) *sp;
			*dp ++ = '\0';
		}
		*dp = '\0';
		return dp - dst;
	}
	return src_len * 2;
}

size_t ansi_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	byte *sp = src, *se = src + src_len;
	if( src == NULL )
		return 0;
	dst_len --;
	if( dst != NULL ) {
		byte *dp = dst, *de = dst + dst_len;
		for( ; sp < se && dp < de; sp ++ ) {
			*dp ++ = '\0';
			*dp ++ = (byte) *sp;
		}
		*dp = '\0';
		return dp - dst;
	}
	return src_len * 2;
}

INLINE size_t ansi_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	byte *sp = src, *se = src + src_len;
	if( src == NULL )
		return 0;
	if( dst != NULL ) {
		byte *dp = dst, *de = dst + dst_len;
		uint c;
		for( ; sp < se && dp < de; sp ++ ) {
			if( (c = *sp) <= 0x7F ) {
				*dp ++ = (byte) c;
			}
			else {
				if( dp + 2 >= de )
					break;
				*dp ++ = (byte) (0xC0 | (c >> 6));
				*dp ++ = (byte) (0x80 | (c & 0x3F));
			}
		}
		*dp = '\0';
		return dp - dst;
	}
	else {
		size_t count = 0;
		for( ; sp < se; sp ++ ) {
			if( *sp > 0x7F )
				count ++;
			count ++;
		}
		return count;
	}
}

size_t ucs2le_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	uint c;
	if( src == NULL )
		return 0;
	src_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			c = (src[i + 1] << 8) | src[i];
			i += 2;
			if( c <= 0x7F ) {
				dst[count ++] = (byte) c;
			}
			else
			if( c > 0x7FF ) {
				if( count + 3 > dst_len )
					break;
				dst[count ++] = (byte) (0xE0 | (c >> 12));
				dst[count ++] = (byte) (0x80 | ((c >> 6) & 0x3F));
				dst[count ++] = (byte) (0x80 | (c & 0x3F));
			}
			else {
				if( count + 2 > dst_len )
					break;
				dst[count ++] = (byte) (0xC0 | (c >> 6));
				dst[count ++] = (byte) (0x80 | (c & 0x3F));
			}
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
	}
	else {
		while( i < src_len ) {
			c = (src[i + 1] << 8) | src[i];
			i += 2;
			if( c > 0x7F ) {
				if( c > 0x7FF )
					count ++;
				count ++;
			}
			count ++;
		}
	}
	return count;
}

size_t ucs2le_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	if( src == NULL )
		return 0;
	src_len --;
	dst_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			dst[count ++] = src[i + 1];
			dst[count ++] = src[i];
			i += 2;
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
		return count;
	}
	return src_len;
}

size_t ucs2le_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	if( src == NULL )
		return 0;
	src_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			dst[count ++] = src[i];
			i += 2;
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
		return count;
	}
	return src_len;
}

size_t ucs2be_to_utf8(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	uint c;
	if( src == NULL )
		return 0;
	src_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			c = (src[i] << 8) | src[i + 1];
			i += 2;
			if( c <= 0x7F ) {
				dst[count ++] = (byte) c;
			}
			else
			if( c > 0x7FF ) {
				if( count + 3 > dst_len )
					break;
				dst[count ++] = (byte) (0xE0 | (c >> 12));
				dst[count ++] = (byte) (0x80 | ((c >> 6) & 0x3F));
				dst[count ++] = (byte) (0x80 | (c & 0x3F));
			}
			else {
				if( count + 2 > dst_len )
					break;
				dst[count ++] = (byte) (0xC0 | (c >> 6));
				dst[count ++] = (byte) (0x80 | (c & 0x3F));
			}
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
	}
	else {
		while( i < src_len ) {
			c = (src[i] << 8) | src[i + 1];
			i += 2;
			if( c > 0x7F ) {
				if( c > 0x7FF )
					count ++;
				count ++;
			}
			count ++;
		}
	}
	return count;
}

size_t ucs2be_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	if( src == NULL )
		return 0;
	src_len --;
	dst_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			dst[count ++] = src[i + 1];
			dst[count ++] = src[i];
			i += 2;
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
		return count;
	}
	return src_len;
}

size_t ucs2be_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	if( src == NULL )
		return 0;
	src_len --;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			dst[count ++] = src[i + 1];
			i += 2;
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
		return count;
	}
	return src_len;
}

size_t utf8_to_ansi(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	uint c, wc;
	if( src == NULL )
		return 0;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			wc = src[i ++];
			if( wc & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				wc &= 0x3F;
				if( wc & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
					wc = (wc << 6) | (c & 0x3F);
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
				/*
				c = (wc << 6) | (c & 0x3F);
				printf( "set1 ucs %x %x\n", c & 0xff, (c >> 8) & 0xff );
				*/
				dst[count] = (char) ((wc << 6) | (c & 0x3F));
			}
			else {
				//printf( "set2 ucs %x\n", wc );
				dst[count] = (char) wc;
			}
			count ++;
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
	}
	else {
		while( i < src_len ) {
			c = src[i ++];
			if( c & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				c &= 0x3F;
				if( c & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
			}
			count ++;
		}
	}
	return count;
}

size_t utf8_to_ucs2le(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	uint c, wc;
	if( src == NULL )
		return 0;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			wc = src[i ++];
			if( wc & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				wc &= 0x3F;
				if( wc & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
					wc = (wc << 6) | (c & 0x3F);
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
				c = (wc << 6) | (c & 0x3F);
				if( count + 2 > dst_len )
					break;
				dst[count ++] = c & 0xFF;
				dst[count ++] = (c >> 8) & 0xFF;
			}
			else {
				if( count + 2 > dst_len )
					break;
				dst[count ++] = wc & 0xFF;
				dst[count ++] = 0;
			}
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
	}
	else {
		while( i < src_len ) {
			c = src[i ++];
			if( c & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				c &= 0x3F;
				if( c & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
			}
			count += 2;
		}
	}
	return count;
}

/* utf-8 to unicode/widechar */
size_t utf8_to_ucs2be(
	byte *dst, size_t dst_len, byte *src, size_t src_len
) {
	size_t i = 0, count = 0;
	uint c, wc;
	if( src == NULL )
		return 0;
	if( dst != NULL ) {
		while( i < src_len && count < dst_len ) {
			wc = src[i ++];
			if( wc & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				wc &= 0x3F;
				if( wc & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
					wc = (wc << 6) | (c & 0x3F);
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
				c = (wc << 6) | (c & 0x3F);
				if( count + 2 > dst_len )
					break;
				dst[count ++] = (c >> 8) & 0xFF;
				dst[count ++] = c & 0xFF;
			}
			else {
				if( count + 2 > dst_len )
					break;
				dst[count ++] = 0;
				dst[count ++] = wc & 0xFF;
			}
		}
		if( count >= dst_len )
			count = dst_len - 1;
		dst[count] = '\0';
	}
	else {
		while( i < src_len ) {
			c = src[i ++];
			if( c & 0x80 ) {
				if( i >= src_len )
					return CS_CNV_ERROR;
				c &= 0x3F;
				if( c & 0x20 ) {
					c = src[i ++];
					if( (c & 0xC0) != 0x80 || i >= src_len )
						return CS_CNV_ERROR;
				}
				c = src[i ++];
				if( (c & 0xC0) != 0x80 )
					return CS_CNV_ERROR;
			}
			count += 2;
		}
	}
	return count;
}

INLINE size_t utf8_strlen( const char *str ) {
	size_t count;
	unsigned int wc;
	for( count = 0; *str != '\0'; str ++, count ++ ) {
		if( *str & 0x80 ) {
			wc = *str & 0x3F;
			if( wc & 0x20 ) {
				str ++;
				count ++;
			}
			str ++;
			count ++;
		}
	}
	return count;
}

INLINE const char *get_charset_name( enum n_charset cs ) {
	switch( cs ) {
	case CS_UTF8:
		return "UTF-8";
	case CS_UCS2BE:
		return "UNICODE";
	case CS_UCS2LE:
		return "UTF-16LE";
	case CS_ANSI:
		return "ANSI";
	default:
		return "unknown";
	}
}

INLINE enum n_charset get_charset_id( const char *name ) {
	if( name == NULL )
		return CS_ANSI;
	if( my_stricmp( name, "UTF8" ) == 0 ||
		my_stricmp( name, "UTF-8" ) == 0
	) {
		return CS_UTF8;
	}
	if( my_stricmp( name, "UTF16" ) == 0 ||
		my_stricmp( name, "UTF-16" ) == 0 ||
		my_stricmp( name, "UCS2" ) == 0 ||
		my_stricmp( name, "UNICODE" ) == 0
	) {
		return CS_UTF16;
	}
	if( my_stricmp( name, "UCS2BE" ) == 0 ||
		my_stricmp( name, "UTF-16BE" ) == 0 ||
		my_stricmp( name, "UTF16BE" ) == 0
	) {
		return CS_UCS2BE;
	}
	if( my_stricmp( name, "UCS2LE" ) == 0 ||
		my_stricmp( name, "UTF-16LE" ) == 0 ||
		my_stricmp( name, "UTF16LE" ) == 0
	) {
		return CS_UCS2LE;
	}
	if( my_stricmp( name, "ANSI" ) == 0 ||
		my_stricmp( name, "ASCII" ) == 0
	) {
		return CS_ANSI;
	}
	return CS_UNKNOWN;
}

INLINE size_t charset_convert(
	char *src, size_t src_len, enum n_charset cs_src, enum n_charset cs_dst,
	char **p_dst
) {
	size_t len = 0;
	if( ! src_len )
		return 0;
#ifdef CSV_DEBUG
	_debug( "charset_convert from %d to %d\n", (int) cs_src, (int) cs_dst );
#endif
	if( cs_src == cs_dst )
		goto noconv;
	switch( cs_dst ) {
	case CS_UTF8:
		len = src_len * 3 + 1;
		Renew( (*p_dst), len, char );
		switch( cs_src ) {
		case CS_ANSI:
			len = ansi_to_utf8( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2LE:
			len = ucs2le_to_utf8( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2BE:
			len = ucs2be_to_utf8( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		default:
			goto noconv;
		}
		break;
	case CS_ANSI:
		len = src_len + 1;
		Renew( (*p_dst), len, char );
		switch( cs_src ) {
		case CS_UTF8:
			len = utf8_to_ansi( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2LE:
			len = ucs2le_to_ansi( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2BE:
			len = ucs2be_to_ansi( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		default:
			goto noconv;
		}
		break;
	case CS_UCS2LE:
		len = src_len * 2 + 1;
		Renew( (*p_dst), len, char );
		switch( cs_src ) {
		case CS_ANSI:
			len = ansi_to_ucs2le( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UTF8:
			len = utf8_to_ucs2le( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2BE:
			len = ucs2be_to_ucs2le( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		default:
			goto noconv;
		}
		break;
	case CS_UCS2BE:
		len = src_len * 2 + 1;
		Renew( (*p_dst), len, char );
		switch( cs_src ) {
		case CS_ANSI:
			len = ansi_to_ucs2be( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UTF8:
			len = utf8_to_ucs2be( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		case CS_UCS2LE:
			len = ucs2le_to_ucs2be( (byte *) (*p_dst), len, (byte *) src, src_len );
			break;
		default:
			goto noconv;
		}
		break;
	default:
		goto noconv;
	}
	if( len == CS_CNV_ERROR )
		return CS_CNV_ERROR;
	if( len > 0 )
		Renew( (*p_dst), len + 1, char );
	return len;
noconv:
	Renew( (*p_dst), src_len + 1, char );
	Copy( src, (*p_dst), src_len, char );
	(*p_dst)[src_len] = '\0';
	return src_len;
	/*
error:
	return CS_CNV_ERROR;
	*/
}

INLINE SV *
charset_quote( enum n_charset cs, const char *str, size_t str_len ) {
	const char *sp = str, *se = str + str_len;
	char *tmp, *so;
	SV *sv;
	uint wc;
	Newx( tmp, str_len * 4 + 5, char );
	so = tmp;
	switch( cs ) {
	case CS_ANSI:
	default:
		*so ++ = '\'';
		for( ; sp < se; sp ++ ) {
			if( *sp == '\'' ) {
				*so ++ = '\'';
				*so ++ = '\'';
			}
			else {
				*so ++ = *sp;
			}
		}
		*so ++ = '\'';
		break;
	case CS_UTF8:
		*so ++ = '\'';
		for( ; sp < se; sp ++ ) {
			wc = (unsigned char) *sp;
			if( wc & 0x80 ) {
				*so ++ = (char) wc;
				wc &= 0x3F;
				if( wc & 0x20 ) {
					if( ++ sp == se )
						break;
					*so ++ = *sp;
				}
				if( ++ sp == se )
					break;
				*so ++ = *sp;
			}
			else if( wc == '\'' ) {
				*so ++ = '\'';
				*so ++ = '\'';
			}
			else {
				*so ++ = (char) wc;
			}
		}
		*so ++ = '\'';
		break;
	case CS_UCS2BE:
		if( (str_len % 2) != 0 )
			goto exit;
		*so ++ = '\0';
		*so ++ = '\'';
		for( ; sp < se; sp ++ ) {
			if( *sp == '\0' ) {
				*so ++ = '\0', sp ++;
				if( *sp == '\'' ) {
					*so ++ = '\'';
					*so ++ = '\0';
					*so ++ = '\'';
				}
				else {
					*so ++ = *sp;
				}
			}
			else {
				*so ++ = *sp ++;
				*so ++ = *sp;
			}
		}
		*so ++ = '\0';
		*so ++ = '\'';
		break;
	case CS_UCS2LE:
		if( (str_len % 2) != 0 )
			goto exit;
		*so ++ = '\'';
		*so ++ = '\0';
		for( ; sp < se; sp ++ ) {
			if( *sp == '\'' ) {
				*so ++ = '\'', sp ++;
				if( *sp == '\0' ) {
					*so ++ = '\0';
					*so ++ = '\'';
					*so ++ = '\0';
				}
				else {
					*so ++ = *sp;
				}
			}
			else {
				*so ++ = *sp ++;
				*so ++ = *sp;
			}
		}
		*so ++ = '\'';
		*so ++ = '\0';
		break;
	}
exit:
	sv = newSVpvn( tmp, so - tmp );
	Safefree( tmp );
	return sv;
}

INLINE SV *
charset_quote_id( enum n_charset cs, const char **args, int argc ) {
	SV *sv;
	char *tmp, *so, *se;
	const char *sp;
	size_t len = 138 + argc * 32;
	int i;
	unsigned int wc;
	Newx( tmp, len, char );
	so = tmp;
	se = tmp + len - 10;
	switch( cs ) {
	case CS_ANSI:
	default:
		for( i = 0; i < argc; i ++ ) {
			sp = args[i];
			if( *sp == '\0' )
				continue;
			*so ++ = '\"';
			while( 1 ) {
				wc = *sp ++;
				if( so >= se ) {
					Renew( tmp, len + 138, char );
					so = tmp + len;
					se = so + 128;
				}
				if( wc == '\0' ) {
					*so ++ = '"';
					break;
				}
				else if( wc == '\"' ) {
					*so ++ = '\"';
					*so ++ = '\"';
				}
				else {
					*so ++ = (char) wc;
				}
			}
			*so ++ = '.'; 
		}
		so --;
		break;
	case CS_UTF8:
		for( i = 0; i < argc; i ++ ) {
			sp = args[i];
			if( *sp == '\0' )
				continue;
			*so ++ = '\"';
			while( 1 ) {
				wc = *sp ++;
				if( so >= se ) {
					Renew( tmp, len + 138, char );
					so = tmp + len;
					se = so + 128;
				}
				if( wc == '\0' ) {
					*so ++ = '"';
					break;
				}
				else if( wc == '\"' ) {
					*so ++ = '\"';
					*so ++ = '\"';
				}
				else
				if( wc & 0x80 ) {
					*so ++ = (char) wc;
					wc &= 0x3F;
					if( wc & 0x20 ) {
						if( *sp == '\0' )
							break;
						*so ++ = *sp ++;
					}
					if( *sp == '\0' )
						break;
					*so ++ = *sp ++;
				}
				else {
					*so ++ = (char) wc;
				}
			}
			*so ++ = '.'; 
		}
		so --;
		break;
	case CS_UCS2LE:
		for( i = 0; i < argc; i ++ ) {
			sp = args[i];
			if( *sp == '\0' )
				continue;
			*so ++ = '\"';
			*so ++ = '\0';
			while( 1 ) {
				wc = *sp ++;
				if( so >= se ) {
					Renew( tmp, len + 138, char );
					so = tmp + len;
					se = so + 128;
				}
				if( wc == '\0' ) {
					if( *sp == '\0' ) {
						*so ++ = '"';
						*so ++ = '\0';
						break;
					}
					else {
						*so ++ = '\0';
						*so ++ = *sp ++;
					}
				}
				else if( wc == '\"' ) {
					if( *sp == '\0' ) {
						*so ++ = '\"';
						*so ++ = '\0';
						*so ++ = '\"';
						*so ++ = '\0';
						sp ++;
					}
					else {
						*so ++ = '\"';
						*so ++ = *sp ++;
					}
				}
				else {
					*so ++ = (char) wc;
					*so ++ = *sp ++;
				}
			}
			*so ++ = '.'; 
			*so ++ = '\0'; 
		}
		so -= 2;
		break;
	case CS_UCS2BE:
#ifdef CSV_DEBUG
		_debug( "quote id ucs2be\n" );
#endif
		for( i = 0; i < argc; i ++ ) {
			sp = args[i];
			if( sp[0] == '\0' && sp[1] == '\0' )
				continue;
			*so ++ = '\0';
			*so ++ = '\"';
			while( 1 ) {
				wc = *sp ++;
				if( so >= se ) {
					Renew( tmp, len + 138, char );
					so = tmp + len;
					se = so + 128;
				}
				if( wc == '\0' ) {
					if( *sp == '\0' ) {
						*so ++ = '"';
						*so ++ = '\0';
						break;
					}
					else if( *sp == '\"' ) {
						*so ++ = '\0';
						*so ++ = '\"';
						*so ++ = '\0';
						*so ++ = '\"';
						sp ++;
					}
					else {
						*so ++ = '\0';
						*so ++ = *sp ++;
					}
				}
				else {
					*so ++ = (char) wc;
					*so ++ = *sp ++;
				}
			}
			*so ++ = '\0'; 
			*so ++ = '.'; 
		}
		so -= 2;
		break;
	}
	if( so >= tmp )
		sv = newSVpvn( tmp, so - tmp );
	else
		sv = newSV( 0 );
	Safefree( tmp );
	return sv;
}
