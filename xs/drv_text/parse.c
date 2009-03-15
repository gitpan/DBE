#include "parse.h"

typedef struct st_keyword {
	const char *name;
	int len;
	int token_type;
} keyword_t;

const keyword_t Keywords[] = {
	{ "AS", 2, TK_ALIAS },
	{ "BY", 2, TK_BY },
	{ "IF", 2, TK_IF },
	{ "IN", 2, TK_IN },
	{ "IS", 2, TK_IS },
	{ "ON", 2, TK_ON },
	{ "OR", 2, TK_OR },
	{ "ABS", 3, TK_ABS },
	{ "AND", 3, TK_AND },
	{ "ASC", 3, TK_ASC },
	{ "AVG", 3, TK_AVG },
	{ "FOR", 3, TK_FOR },
	{ "MAX", 3, TK_MAX },
	{ "MIN", 3, TK_MIN },
	{ "MOD", 3, TK_MOD },
	{ "NOT", 3, TK_NOT },
	{ "NOW", 3, TK_CURRENT_TIMESTAMP },
	{ "SET", 3, TK_SET },
	{ "SUM", 3, TK_SUM },
	{ "BOTH", 4, TK_BOTH },
	{ "DESC", 4, TK_DESC },
	{ "DROP", 4, TK_DROP },
	{ "FROM", 4, TK_FROM },
	{ "INTO", 4, TK_INTO },
	{ "JOIN", 4, TK_JOIN },
	{ "LEFT", 4, TK_LEFT },
	{ "LIKE", 4, TK_LIKE },
	{ "NULL", 4, TK_NULL },
	{ "SHOW", 4, TK_SHOW },
	{ "TRIM", 4, TK_TRIM },
	{ "ASCII", 5, TK_ASCII },
	{ "COUNT", 5, TK_COUNT },
	{ "CROSS", 5, TK_CROSS },
	{ "GROUP", 5, TK_GROUP },
	{ "INNER", 5, TK_INNER },
	{ "LCASE", 5, TK_LOWER },
	{ "LIMIT", 5, TK_LIMIT },
	{ "LOWER", 5, TK_LOWER },
	{ "LTRIM", 5, TK_LTRIM },
	{ "ORDER", 5, TK_ORDER },
	{ "ROUND", 5, TK_ROUND },
	{ "RTRIM", 5, TK_RTRIM },
	{ "TABLE", 5, TK_TABLE },
	{ "UCASE", 5, TK_UPPER },
	{ "UPPER", 5, TK_UPPER },
	{ "USING", 5, TK_USING },
	{ "WHERE", 5, TK_WHERE },
	{ "CREATE", 6, TK_CREATE },
	{ "CONCAT", 6, TK_CONCAT },
	{ "DELETE", 6, TK_DELETE },
	{ "EXISTS", 6, TK_EXISTS },
	{ "INSERT", 6, TK_INSERT },
	{ "LENGTH", 6, TK_LENGTH },
	{ "LOCATE", 6, TK_LOCATE },
	{ "OFFSET", 6, TK_OFFSET },
	{ "SELECT", 6, TK_SELECT },
	{ "SUBSTR", 6, TK_SUBSTR },
	{ "UPDATE", 6, TK_UPDATE },
	{ "VALUES", 6, TK_VALUES },
	{ "BETWEEN", 7, TK_BETWEEN },
	{ "CONVERT", 7, TK_CONVERT },
	{ "DEFAULT", 7, TK_DEFAULT },
	{ "LEADING", 7, TK_LEADING },
	{ "POSITION", 8, TK_POSITION },
	{ "TRAILING", 8, TK_TRAILING },
	{ "SUBSTRING", 9, TK_SUBSTR },
	{ "VARIABLES", 9, TK_VARIABLES },
	{ "CHAR_LENGTH", 11, TK_CHAR_LENGTH },
	{ "CURRENT_TIME", 12, TK_CURRENT_TIME },
	{ "CURRENT_DATE", 12, TK_CURRENT_DATE },
	{ "OCTET_LENGTH", 12, TK_LENGTH },
	{ "CHARACTER_LENGTH", 16, TK_CHAR_LENGTH },
	{ "CURRENT_TIMESTAMP", 17, TK_CURRENT_TIMESTAMP }
};

const keyword_t *KwEnd = Keywords + (sizeof(Keywords) / sizeof(keyword_t));

/*
const short int KeywordIndex[] = {
	-1, -1, 0, 7, 19, 30, 47, 59, 63, 65, -1, 67, 68, -1, -1, -1, 71, 72,
};
*/

const char IsIdChar[] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 2x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
};
#define IdChar(C)  (((c = C) & 0x80) != 0 || (c > 0x1f && IsIdChar[c - 0x20]))

INLINE int get_kw_token( const char *z, int len ) {
	int ch;
	const keyword_t *kw;
	const char *sp, *se = z + len, *dp;
	for( kw = Keywords; kw < KwEnd; kw ++ ) {
		if( kw->len == len ) {
			for( sp = z, dp = kw->name; sp < se; sp ++, dp ++ ) {
				ch = TOUPPER( *sp );
				if( *dp > ch )
					return TK_ID;
				if( *dp != ch )
					goto next_kw;
			}
#ifdef CSV_DEBUG
			_debug( "keyword [%s]\n", kw->name );
#endif
			return kw->token_type;
		}
		else if( kw->len > len )
			break;
next_kw:
		continue;
	}
	return TK_ID;
}

INLINE int get_token( const char *z, int *token_type ) {
	int c;
	const char *s;
	switch( *z ) {
	case '(':
		*token_type = TK_LP;
		return 1;
	case ')':
		*token_type = TK_RP;
		return 1;
	case ';':
		*token_type = TK_SEMI;
		return 1;
	case ',':
		*token_type = TK_COMMA;
		return 1;
	case '.':
		*token_type = TK_DOT;
		return 1;
	case '+':
		*token_type = TK_ADD;
		return 1;
	case '-':
		*token_type = TK_SUB;
		return 1;
	case '*':
		*token_type = TK_MUL;
		return 1;
	case '/':
		*token_type = TK_DIV;
		return 1;
	case '?':
		*token_type = TK_QUEST;
		return 1;
	case '<':
		if( z[1] == '>' ) {
			*token_type = TK_NE;
			return 2;
		}
		else if( z[1] == '=' ) {
			*token_type = TK_LE;
			return 2;
		}
		else if( z[1] == '<' ) {
			*token_type = TK_SHL;
			return 2;
		}
		*token_type = TK_LT;
		return 1;
	case '>':
		if( z[1] == '=' ) {
			*token_type = TK_GE;
			return 2;
		}
		else if( z[1] == '>' ) {
			*token_type = TK_SHR;
			return 2;
		}
		*token_type = TK_GT;
		return 1;
	case '!':
		*token_type = TK_NE;
		return 1;
	case '=':
		*token_type = TK_EQ;
		return 1;
	case '&':
		if( z[1] == '&' ) {
			*token_type = TK_AND;
			return 2;
		}
		*token_type = TK_BITAND;
		return 1;
	case '|':
		if( z[1] == '|' ) {
			*token_type = TK_OR;
			return 2;
		}
		*token_type = TK_BITOR;
		return 1;
	case '^':
		*token_type = TK_EXP;
		return 1;
	case '"':
	case '\'':
	case '[':
		for( c = *z, s = z + 1; ; s ++ ) {
			if( *s == '\0' ) {
				*token_type = 0;
				return (int) (s - z);
			}
	        else if( *s == '\\' ) {
	        	s ++;
	        }
	        else if( *s == c ) {
          		if( s[1] == c )
            		s ++;
            	else
            		break;
            }
		}
		*token_type = c == '\'' ? TK_ARG : TK_QID;
		return (int) (s - z) + 1;
	case '0':
		if( z[1] == 'x' ) {
			for( s = z + 2; isXDIGIT( *s ); s ++ );
			*token_type = TK_HEX;
			return (int) (s - z);
		}
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		for( s = z + 1; isDIGIT( *s ); s ++ );
		if( *s == '.' ) {
			*token_type = TK_FLOAT;
			for( s ++; isDIGIT( *s ); s ++ );
		}
		else
			*token_type = TK_INT;
		return (int) (s - z);
	default:
		if( ! IdChar( *z ) )
			break;
		for( s = z + 1; IdChar( *s ); s ++ );
		c = (int) (s - z);
		*token_type = get_kw_token( z, c );
		return c;
	}
	*token_type = TK_ILLEGAL;
	return 1;
}

INLINE int run_parser( csv_parse_t *parse, const char *sql, size_t sql_len ) {
	void* engine = (void *) ParseAlloc( malloc );
	const char *sp, *se;
	Token *tk = &parse->last_token;
	int token_type = TK_ILLEGAL;
#ifndef NDEBUG
	ParseTrace( stderr, "[csv_lemon] " );
#endif
#ifdef CSV_DEBUG
	_debug( "SQL [%s]\n", sql );
#endif
	if( sql_len == 0 )
		sql_len = strlen( sql );
	Renew( parse->sql, sql_len + 1, char );
	Copy( sql, parse->sql, sql_len, char );
	parse->sql[sql_len] = '\0';
	parse->sql_len = sql_len;
	sql = parse->sql;
	for( sp = sql, se = sql + sql_len; sp < se; ) {
		for( ; isSPACE( *sp ); sp ++ );
	    tk->str = sp;
	    tk->len = get_token( sp, &token_type );
	    sp += tk->len;
	    switch( token_type ) {
	    case TK_ILLEGAL:
#ifdef CSV_DEBUG
	    	_debug( "Illegal token at \"%s\"\n", tk->str );
#endif
	    	csv_error_msg(
	    		parse, CSV_ERR_SYNTAX, "Illegal token at '%s'", tk->str );
	    	goto abort_parse;
	    }
	    Parse( engine, token_type, *tk, parse );
	    if( parse->csv->last_errno )
	    	goto abort_parse;
		for( ; isSPACE( *sp ); sp ++ );
	}
	if( sp > sql && sp[-1] != ';' )
	    Parse( engine, TK_SEMI, *tk, parse );
	ParseFree( engine, free );
	return CSV_OK;
abort_parse:
	ParseFree( engine, free );
	Safefree( parse->sql );
	Safefree( parse->qmark );
	return CSV_ERROR;
}
