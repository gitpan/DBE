#include "parse_int.h"

INLINE void fnc_concat( Expr *x ) {
	size_t len = 0, i, p;
	ExprList *arg = x->list;
	Expr *x1;
#ifdef CSV_DEBUG
	_debug( "function CONCAT exprlist 0x%x\n", arg );
#endif
	for( i = 0; i < arg->expr_count; i ++ ) {
		x1 = arg->expr[i];
		VAR_EVAL_SV( x1->var );
		if( ! x1->var.flags ) {
			x->var.flags = 0;
			return;
		}
		len += x1->var.sv_len;
	}
	Renew( x->var.sv, len + 1, char );
	for( i = 0, p = 0; i < arg->expr_count; i ++ ) {
		x1 = arg->expr[i];
		Copy( x1->var.sv, &x->var.sv[p], x1->var.sv_len, char );
		p += x1->var.sv_len;
	}
	x->var.sv_len = len + 1;
	x->var.sv[len] = '\0';
	VAR_FLAG_SV( x->var );
}

INLINE void fnc_abs( Expr *x ) {
	Expr *a = x->left;
#ifdef CSV_DEBUG
	_debug( "function ABS expr 0x%x\n", a );
#endif
	VAR_EVAL_NUM( a->var );
	if( a->var.flags & VAR_HAS_IV ) {
		if( a->var.iv < 0 )
			x->var.iv = - a->var.iv;
		else
			x->var.iv = a->var.iv;
		VAR_FLAG_IV( x->var );
	}
	else {
		if( a->var.nv < 0 )
			x->var.nv = - a->var.nv;
		else
			x->var.nv = a->var.nv;
		VAR_FLAG_NV( x->var );
	}
}

INLINE void fnc_round( Expr *x ) {
	Expr *a = x->left;
	int prec = 0;
#ifdef CSV_DEBUG
	_debug( "function ROUND expr 0x%x\n", a );
#endif
	VAR_EVAL_NUM( a->var );
	if( a->var.flags & VAR_HAS_IV )
		return;
	if( x->right != NULL ) {
		Expr *b = x->right;
		VAR_EVAL_IV( b->var );
		prec = b->var.iv;
	}
	a->var.nv = my_round( a->var.nv, prec );
}

INLINE void fnc_current_timestamp( Expr *x ) {
	time_t now = time( NULL );
	struct tm *tim = localtime( &now );
	//2007-10-02 12:12:12
	Renew( x->var.sv, 20, char );
	x->var.sv_len = strftime( x->var.sv, 20, "%Y-%m-%d %H:%M:%S", tim );
	VAR_FLAG_SV( x->var );
}

INLINE void fnc_current_time( Expr *x ) {
	time_t now = time( NULL );
	struct tm *tim = localtime( &now );
	//12:12:12
	Renew( x->var.sv, 9, char );
	x->var.sv_len = strftime( x->var.sv, 9, "%H:%M:%S", tim );
	VAR_FLAG_SV( x->var );
}

INLINE void fnc_current_date( Expr *x ) {
	time_t now = time( NULL );
	struct tm *tim = localtime( &now );
	//2007-10-02
	Renew( x->var.sv, 11, char );
	x->var.sv_len = strftime( x->var.sv, 11, "%Y-%m-%d", tim );
	VAR_FLAG_SV( x->var );
}

INLINE void fnc_lower( Expr *x, enum n_charset cs ) {
	Expr *a = x->left;
#ifdef CSV_DEBUG
	_debug( "function LOWER expr 0x%x\n", a );
#endif
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		if( cs == CS_UTF8 ) {
			size_t len = a->var.sv_len * 2 + 1;
			Renew( x->var.sv, len, char );
			x->var.sv_len = g_dbe->utf8_tolower(
				a->var.sv, a->var.sv_len, x->var.sv, len );
			Renew( x->var.sv, x->var.sv_len + 1, char );
		}
		else {
			char *sp = a->var.sv, *dp, *se = sp + a->var.sv_len;
			Renew( x->var.sv, a->var.sv_len + 1, char );
			for( dp = x->var.sv; sp < se; *dp ++ = TOLOWER( *sp ++ ) );
			*dp = '\0';
			x->var.sv_len = a->var.sv_len;
		}
		x->var.flags = VAR_HAS_SV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_upper( Expr *x, enum n_charset cs ) {
	Expr *a = x->left;
#ifdef CSV_DEBUG
	_debug( "function UPPER expr 0x%x\n", a );
#endif
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		if( cs == CS_UTF8 ) {
			size_t len = a->var.sv_len * 2 + 1;
			Renew( x->var.sv, len, char );
			x->var.sv_len = g_dbe->utf8_toupper(
				a->var.sv, a->var.sv_len, x->var.sv, len );
			Renew( x->var.sv, x->var.sv_len + 1, char );
		}
		else {
			char *sp = a->var.sv, *dp, *se = sp + a->var.sv_len;
			Renew( x->var.sv, a->var.sv_len + 1, char );
			for( dp = x->var.sv; sp < se; *dp ++ = TOUPPER( *sp ++ ) );
			*dp = '\0';
			x->var.sv_len = a->var.sv_len;
		}
		x->var.flags = VAR_HAS_SV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_trim( Expr *x ) {
	Expr *a = x->left;
	Expr *b = x->right;
	Expr *c = b->left;
	char *s1, *s2, *s3, *rms, *rme, *rmp;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		if( c != NULL ) {
			VAR_EVAL_SV( c->var );
			rms = c->var.sv;
			rme = rms + c->var.sv_len - 1;
			if( rme < rms ) {
				VAR_COPY( a->var, x->var );
				return;
			}
		}
		else {
			rms = " ";
			rme = rms;
		}
		s1 = a->var.sv;
		s2 = a->var.sv + a->var.sv_len - 1;
		if( b->op == TK_LEADING || b->op == TK_BOTH ) {
			for( s3 = s1, rmp = rms; s3 <= s2; s3 ++, rmp ++ ) {
				if( *s3 != *rmp )
					break;
				if( rmp == rme ) {
					s1 = s3;
					rmp = rms;
				}
			}
		}
		if( b->op == TK_TRAILING || b->op == TK_BOTH ) {
			for( s3 = s2, rmp = rme; s3 >= s1; s3 --, rmp -- ) {
				if( *s3 != *rmp )
					break;
				if( rmp == rms ) {
					s2 = s3;
					rmp = rme;
				}
			}
		}
		if( s1 <= s2 ) {
			x->var.sv_len = s2 - s1 + 1;
			Renew( x->var.sv, x->var.sv_len + 1, char );
			Copy( s1, x->var.sv, x->var.sv_len, char );
			x->var.sv[x->var.sv_len] = '\0';
		}
		else {
			x->var.sv_len = 0;
			Renew( x->var.sv, 1, char );
			x->var.sv[0] = '\0';
		}
		x->var.flags = VAR_HAS_SV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_ltrim( Expr *x ) {
	Expr *a = x->left;
	char *s1, *s2;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		s1 = a->var.sv;
		s2 = a->var.sv + a->var.sv_len - 1;
		for( ; s1 <= s2 && isSPACE( *s1 ); s1 ++ );
		if( s1 <= s2 ) {
			x->var.sv_len = s2 - s1 + 1;
			Renew( x->var.sv, x->var.sv_len + 1, char );
			Copy( s1, x->var.sv, x->var.sv_len, char );
			x->var.sv[x->var.sv_len] = '\0';
		}
		else {
			x->var.sv_len = 0;
			Renew( x->var.sv, 1, char );
			x->var.sv[0] = '\0';
		}
		x->var.flags = VAR_HAS_SV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_rtrim( Expr *x ) {
	Expr *a = x->left;
	char *s1, *s2;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		s1 = a->var.sv;
		s2 = a->var.sv + a->var.sv_len - 1;
		for( ; s2 >= s1 && isSPACE( *s2 ); s2 -- );
		if( s1 <= s2 ) {
			x->var.sv_len = s2 - s1 + 1;
			Renew( x->var.sv, x->var.sv_len + 1, char );
			Copy( s1, x->var.sv, x->var.sv_len, char );
			x->var.sv[x->var.sv_len] = '\0';
		}
		else {
			x->var.sv_len = 0;
			Renew( x->var.sv, 1, char );
			x->var.sv[0] = '\0';
		}
		x->var.flags = VAR_HAS_SV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_octet_length( Expr *x, enum n_charset cs ) {
	Expr *a = x->left;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		if( cs == CS_UTF8 )
			x->var.iv = (long) utf8_strlen( a->var.sv );
		else
			x->var.iv = (long) a->var.sv_len;
		x->var.flags = VAR_HAS_IV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_char_length( Expr *x ) {
	Expr *a = x->left;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		x->var.iv = (long) a->var.sv_len;
		x->var.flags = VAR_HAS_IV;
	}
	else
		x->var.flags = 0;
}

INLINE void fnc_substr( Expr *x, enum n_charset cs ) {
	Expr *a = x->left;
	int s, l;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		VAR_EVAL_IV( x->list->expr[0]->var );
		if( cs == CS_UTF8 ) {
			int sl = (int) utf8_strlen( a->var.sv ), ch, count = 0;
			char *sp, *se = a->var.sv + a->var.sv_len, *dp;
			s = MIN( x->list->expr[0]->var.iv, sl );
			if( x->list->expr_count > 1 ) {
				VAR_EVAL_IV( x->list->expr[1]->var );
				l = MIN( x->list->expr[1]->var.iv, sl - s );
			}
			else {
				l = (int) a->var.sv_len - s;
			}
			if( l > 0 ) {
				Renew( x->var.sv, l * 3 + 1, char );
				/* search offset */
				for( sp = a->var.sv; sp < se; sp ++ ) {
					if( count == s )
						break;
					if( *sp & 0x80 ) {
						ch = *sp & 0x3F;
						if( ch & 0x20 ) {
							sp ++;
						}
						sp ++;
					}
					count ++;
				}
				/* copy string */
				for( count = 1, dp = x->var.sv; sp < se; count ++ ) {
					if( *sp & 0x80 ) {
						ch = *sp & 0x3F;
						if( ch & 0x20 ) {
							*dp ++ = *sp ++;
						}
						*dp ++ = *sp ++;
					}
					*dp ++ = *sp ++;
					if( count == l )
						break;
				}
				*dp = '\0';
				x->var.sv_len = dp - x->var.sv;
				/* truncate at real end */
				Renew( x->var.sv, x->var.sv_len + 1, char );
			}
			else {
				x->var.sv_len = 0;
				Renew( x->var.sv, 1, char );
				x->var.sv[0] = '\0';
			}
		}
		else {
			s = MIN( x->list->expr[0]->var.iv, (int) a->var.sv_len );
			if( x->list->expr_count > 1 ) {
				VAR_EVAL_IV( x->list->expr[1]->var );
				l = MIN( x->list->expr[1]->var.iv, (int) a->var.sv_len - s );
			}
			else {
				l = (int) a->var.sv_len - s;
			}
			if( l > 0 ) {
				x->var.sv_len = l;
				Renew( x->var.sv, l + 1, char );
				Copy( a->var.sv + s, x->var.sv, l, char );
				x->var.sv[l] = '\0';
			}
			else {
				x->var.sv_len = 0;
				Renew( x->var.sv, 1, char );
				x->var.sv[0] = '\0';
			}
		}
		x->var.flags = VAR_HAS_SV;
	}
	else {
		x->var.flags = 0;
	}
}

INLINE void fnc_locate( Expr *x, enum n_charset cs ) {
	Expr *a = x->left;
	Expr *b = x->right;
	long start = 0, wc;
	char *str, *sf;
	if( ! a->var.flags || ! b->var.flags ) {
		x->var.flags = 0;
		return;
	}
	if( x->list != NULL ) {
		VAR_EVAL_IV( x->list->expr[0]->var );
		start = x->list->expr[0]->var.iv;
	}
	VAR_EVAL_SV( a->var );
	VAR_EVAL_SV( b->var );
	str = b->var.sv;
	if( start > 0 ) {
		if( cs == CS_UTF8 ) {
			while( start > 0 ) {
				if( *str & 0x80 ) {
					wc = *str & 0x3F;
					if( wc & 0x20 ) {
						if( ++ *str == '\0' )
							goto notfound;
					}
					if( ++ *str == '\0' )
						goto notfound;
				}
				if( ++ *str == '\0' )
					goto notfound;
				start --;
			}
		}
		else {
			str += start;
		}
		if( (size_t) start >= b->var.sv_len )
			goto notfound;
	}
	if( cs == CS_UTF8 ) {
		char *s1, *s2;
		size_t str_len = b->var.sv_len - (str - b->var.sv);
		size_t s1_len = str_len * 2, s2_len = a->var.sv_len * 2;
		Newx( s1, s1_len + 1, char );
		if( g_dbe->utf8_toupper( str, str_len, s1, s1_len ) <= 0 ) {
			Safefree( s1 );
			goto notfound;
		}
		Newx( s2, s2_len + 1, char );
		if( g_dbe->utf8_toupper( a->var.sv, a->var.sv_len, s2, s2_len ) <= 0 ) {
			Safefree( s1 );
			Safefree( s2 );
			goto notfound;
		}
		sf = strstr( s1, s2 );
		start = sf != NULL ? (long) (sf - s1 + 1) : 0;
		Safefree( s1 );
		Safefree( s2 );
	}
	else {
		sf = my_stristr( str, a->var.sv );
		start = sf != NULL ? (long) (sf - b->var.sv + 1) : 0;
	}
	x->var.iv = start;
	x->var.flags = VAR_HAS_IV;
	return;
notfound:
	x->var.iv = 0;
	x->var.flags = VAR_HAS_IV;
}

INLINE void fnc_ascii( Expr *x ) {
	Expr *a = x->left;
	if( a->var.flags ) {
		VAR_EVAL_SV( a->var );
		x->var.iv = a->var.sv_len ? (unsigned char) a->var.sv[0] : 0;
		x->var.flags = VAR_HAS_IV;
	}
	else
		x->var.flags = 0;
}

INLINE int fnc_convert( csv_parse_t *parse, Expr *x ) {
	Expr *a = x->left;
	Expr *b = x->right;
	Expr *c = b->left;
	/*
#ifdef CSV_DEBUG
	print_expr( x, 0 );
#endif
	*/
	if( c->op == TK_USING ) {
		/* convert charset */
		if( ! b->var.iv ) {
			b->var.iv = (int) get_charset_id( b->var.sv );
			if( b->var.iv == CS_UNKNOWN ) {
				csv_error_msg( parse, CSV_ERR_UNKNOWNCHARSET,
					"Invalid charset in conversation '%s'", b->var.sv );
				return CSV_ERROR;
			}
		}
		VAR_EVAL_SV( a->var );
		x->var.sv_len = charset_convert( a->var.sv, a->var.sv_len,
			parse->csv->client_charset, (enum n_charset) b->var.iv,
			&x->var.sv
		);
		x->var.flags = VAR_HAS_SV;
	}
	else {
		/* convert type */
		if( ! b->var.iv ) {
			if( my_stricmp( b->var.sv, "INTEGER" ) == 0 ||
				my_stricmp( b->var.sv, "SIGNED" ) == 0
			) {
				b->var.iv = TK_SIGNED;
			}
			else if( my_stricmp( b->var.sv, "UNSIGNED" ) == 0 ) {
				b->var.iv = TK_UNSIGNED;
			}
			else if( my_stricmp( b->var.sv, "CHAR" ) == 0 ) {
				b->var.iv = TK_CHAR;
			}
			else {
				csv_error_msg( parse, CSV_ERR_UNKNOWNTYPE,
					"Invalid type in conversation '%s'", b->var.sv );
				return CSV_ERROR;
			}
		}
		switch( b->var.iv ) {
		case TK_SIGNED:
			VAR_EVAL_IV( a->var );
			x->var.iv = a->var.iv;
			x->var.flags = VAR_HAS_IV;
			break;
		case TK_UNSIGNED:
			VAR_EVAL_IV( a->var );
			x->var.iv = (unsigned int) a->var.iv;
			x->var.flags = VAR_HAS_IV;
			break;
		case TK_CHAR:
			VAR_COPY( a->var, x->var );
			VAR_EVAL_SV( x->var );
			break;
		}
	}
	return CSV_OK;
}

INLINE void fnc_avg( Expr *x ) {
	Expr *a = x->left, *b = NULL;
	size_t i;
	if( x->list != NULL ) {
		for( i = 0; i < x->list->expr_count; i ++ ) {
			if( x->list->expr[i]->row == x->row ) {
				b = x->list->expr[i];
				break;
			}
		}
	}
	if( b == NULL ) {
		b = csv_expr( 0, 0, 0, 0, 0 );
		b->row = x->row;
		b->var.flags = VAR_HAS_IV;
		x->list = csv_exprlist_add( x->list, b, 0 );
	}
	if( ! a->var.flags ) {
		//VAR_COPY( b->var, x->var );
		return;
	}
	b->alias_len ++;
	if( b->var.flags & VAR_HAS_IV ) {
		if( a->var.flags & VAR_HAS_IV ) {
			b->var.iv += a->var.iv;
			x->var.iv = (long) floor( b->var.iv / b->alias_len + 0.5 );
			x->var.flags = VAR_HAS_IV;
		}
		else {
			b->var.nv = b->var.iv + a->var.iv;
			b->var.flags = VAR_HAS_NV;
			x->var.nv = b->var.nv / b->alias_len;
			x->var.flags = VAR_HAS_NV;
		}
	}
	else {
		if( a->var.flags & VAR_HAS_IV )
			b->var.nv += a->var.iv;
		else
			b->var.nv += a->var.nv;
		x->var.nv = b->var.nv / b->alias_len;
		x->var.flags = VAR_HAS_NV;
	}
	//print_expr( x, 0 );
}

INLINE void fnc_min( Expr *x ) {
	Expr *a = x->left;
	if( ! a->var.flags )
		return;
	if( ! x->var.flags ) {
		VAR_COPY( a->var, x->var );
		return;
	}
	if( a->var.flags & VAR_HAS_SV ) {
		if( (x->var.flags & VAR_HAS_SV) == 0 )
			VAR_EVAL_SV( x->var );
		if( strcmp( a->var.sv, x->var.sv ) < 0 )
			VAR_COPY( a->var, x->var );
	}
	else if( x->var.flags & VAR_HAS_SV ) {
		if( (a->var.flags & VAR_HAS_SV) == 0 )
			VAR_EVAL_SV( a->var );
		if( strcmp( a->var.sv, x->var.sv ) < 0 )
			VAR_COPY( a->var, x->var );
	}
	else if( a->var.flags & VAR_HAS_NV ) {
		if( (x->var.flags & VAR_HAS_NV) == 0 )
			VAR_EVAL_NV( x->var );
		if( a->var.nv < x->var.nv )
			x->var.nv = a->var.nv;
	}
	else if( x->var.flags & VAR_HAS_NV ) {
		if( (a->var.flags & VAR_HAS_NV) == 0 )
			VAR_EVAL_NV( a->var );
		if( a->var.nv < x->var.nv )
			x->var.nv = a->var.nv;
	}
	else {
		if( a->var.iv < x->var.iv )
			x->var.iv = a->var.iv;
	}
}

INLINE void fnc_max( Expr *x ) {
	Expr *a = x->left;
	if( ! a->var.flags )
		return;
	if( ! x->var.flags ) {
		VAR_COPY( a->var, x->var );
		return;
	}
	if( a->var.flags & VAR_HAS_SV ) {
		if( (x->var.flags & VAR_HAS_SV) == 0 )
			VAR_EVAL_SV( x->var );
		if( strcmp( a->var.sv, x->var.sv ) > 0 )
			VAR_COPY( a->var, x->var );
	}
	else if( x->var.flags & VAR_HAS_SV ) {
		if( (a->var.flags & VAR_HAS_SV) == 0 )
			VAR_EVAL_SV( a->var );
		if( strcmp( a->var.sv, x->var.sv ) > 0 )
			VAR_COPY( a->var, x->var );
	}
	else if( a->var.flags & VAR_HAS_NV ) {
		if( (x->var.flags & VAR_HAS_NV) == 0 )
			VAR_EVAL_NV( x->var );
		if( a->var.nv > x->var.nv )
			x->var.nv = a->var.nv;
	}
	else if( x->var.flags & VAR_HAS_NV ) {
		if( (a->var.flags & VAR_HAS_NV) == 0 )
			VAR_EVAL_NV( a->var );
		if( a->var.nv > x->var.nv )
			x->var.nv = a->var.nv;
	}
	else {
		if( a->var.iv > x->var.iv )
			x->var.iv = a->var.iv;
	}
}

INLINE void fnc_sum( Expr *x ) {
	Expr *a = x->left;
	if( ! a->var.flags )
		return;
	VAR_EVAL_NUM( a->var );
	if( ! x->var.flags ) {
		VAR_COPY( a->var, x->var );
		return;
	}
	if( a->var.flags & VAR_HAS_IV ) {
		if( x->var.flags & VAR_HAS_IV )
			x->var.iv += a->var.iv;
		else
			x->var.nv += a->var.iv;
	}
	else {
		if( x->var.flags & VAR_HAS_IV ) {
			x->var.nv = x->var.iv;
			x->var.flags = (x->var.flags & (~VAR_HAS_IV)) | VAR_HAS_NV;
		}
		x->var.nv += a->var.nv;
	}
}

INLINE void fnc_count( Expr *x ) {
	Expr *a = x->left;
	if( a != NULL ) {
		if( ! a->var.flags )
			return;
	}
	if( ! x->var.flags ) {
		x->var.iv = 1;
		x->var.flags = VAR_HAS_IV;
		return;
	}
	x->var.iv ++;
}
