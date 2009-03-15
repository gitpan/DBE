#include "parse_int.h"

INLINE Expr *csv_expr( int op, Expr *a, Expr *b, ExprList *l, const Token *t ) {
	Expr *x;
	Newxz( x, 1, Expr );
	if( t != NULL ) {
		Newx( x->token, 1, Token );
		x->token->str = t->str;
		if( a != NULL && a->token != NULL ) {
			if( a->token->str < t->str ) {
				x->token->str = a->token->str;
				x->token->len = t->str - a->token->str + t->len;
			}
			else
				x->token->len = a->token->str - x->token->str + a->token->len;
		}
		if( b != NULL && b->token != NULL )
			x->token->len = b->token->str - x->token->str + b->token->len;
		else
			x->token->len = t->len;
	}
	switch( op ) {
	case TK_INT:
	case TK_FLOAT:
		x->var.sv_len = t->len;
		Newx( x->var.sv, x->var.sv_len + 1, char );
		Copy( t->str, x->var.sv, x->var.sv_len, char );
		x->var.sv[x->var.sv_len] = '\0';
		x->var.flags = VAR_HAS_SV;
#ifdef CSV_DEBUG
		_debug( "argument [%s] as 0x%x\n", x->var.sv, x );
#endif
		break;
	case TK_HEX:
		{
			size_t l = t->len / 2;
			const char *s1, *s3;
			char *s2;
			int ch;
			Newx( x->var.sv, l + 1, char );
			s2 = x->var.sv;
			for( s1 = t->str + 2, s3 = t->str + t->len; s1 < s3; ) {
				ch = CHAR_FROM_HEX[*s1 & 0x7f] << 4;
				s1 ++;
				if( s1 < s3 ) {
					ch += CHAR_FROM_HEX[*s1 & 0x7f];
					s1 ++;
				}
				*s2 ++ = (char) ch;
			}
			*s2 = '\0';
			x->var.sv_len = s2 - x->var.sv;
			x->var.flags = VAR_HAS_SV;
		}
		break;
	case TK_ARG:
		x->var.sv_len = t->len - 2;
		Newx( x->var.sv, x->var.sv_len + 1, char );
		Copy( &t->str[1], x->var.sv, x->var.sv_len, char );
		x->var.sv[x->var.sv_len] = '\0';
		x->var.flags = VAR_HAS_SV;
#ifdef CSV_DEBUG
		_debug( "argument [%s] as 0x%x\n", x->var.sv, x );
#endif
		break;
	case TK_ID:
		x->var.sv_len = t->len;
		Newx( x->var.sv, x->var.sv_len + 1, char );
		Copy( t->str, x->var.sv, x->var.sv_len, char );
		x->var.sv[x->var.sv_len] = '\0';
		x->var.flags = VAR_HAS_SV;
		x->flags = EXPR_IS_ID;
#ifdef CSV_DEBUG
		_debug( "identifier [%s] as 0x%x\n", x->var.sv, x );
#endif
		break;
	case TK_QID:
		x->var.sv_len = t->len - 2;
		Newx( x->var.sv, x->var.sv_len + 1, char );
		Copy( &t->str[1], x->var.sv, x->var.sv_len, char );
		x->var.sv[x->var.sv_len] = '\0';
		x->var.flags = VAR_HAS_SV;
		x->flags = EXPR_IS_ID;
#ifdef CSV_DEBUG
		_debug( "identifier [%s] as 0x%x\n", x->var.sv, x );
#endif
		break;
	case TK_PLAIN:
		x->var.sv_len = t->len;
		Newx( x->var.sv, x->var.sv_len + 1, char );
		Copy( t->str, x->var.sv, x->var.sv_len, char );
		x->var.sv[x->var.sv_len] = '\0';
		x->var.flags = VAR_HAS_SV;
		x->left = a;
		x->right = b;
#ifdef CSV_DEBUG
		_debug( "plain [%s] as 0x%x\n", x->var.sv, x );
#endif
		break;
	case TK_ALL:
		x->var.iv = 1;
		x->var.flags = VAR_HAS_IV;
		x->flags = EXPR_IS_ALL;
		x->op = TK_ALL;
#ifdef CSV_DEBUG
		_debug( "all columns [*] as 0x%x\n", x );
#endif
		break;
	case TK_QUEST:
		x->flags = EXPR_IS_QUEST;
#ifdef CSV_DEBUG
		_debug( "question mark [?] as 0x%x\n", x );
#endif
		break;
	case TK_DOT:
		x->left = a;
		x->right = b;
		x->op = TK_DOT;
#ifdef CSV_DEBUG
		_debug( "table.field -> [%s].[%s] as 0x%x\n",
			a->var.sv, b->op == TK_ALL ? "*" : b->var.sv, x );
#endif
		break;
	case TK_LIKE:
	case TK_NOTLIKE:
		b->flags |= EXPR_USE_PATTERN;
		if( (b->flags & EXPR_IS_QUEST) == 0 )
			b->pat = g_dbe->regexp_from_pattern( b->var.sv, b->var.sv_len, 1 );
	case TK_COUNT:
	case TK_SUM:
	case TK_AVG:
	case TK_MAX:
		x->var.flags = VAR_HAS_IV;
	case TK_MIN:
		x->flags = EXPR_IS_AGGR;
	default:
		x->left = a;
		x->right = b;
		x->op = op;
		x->list = l;
#ifdef CSV_DEBUG
		_debug( "a [0x%x] op [%d] b [0x%x] -> x [0x%x]\n", a, op, b, x );
#endif
	}
	return x;
}

INLINE void csv_expr_free( Expr *x ) {
	if( x == NULL )
		return;
#ifdef CSV_DEBUG
	_debug( "freeing expr 0x%x\n", x );
#endif
	if( x->left != NULL )
		csv_expr_free( x->left );
	if( x->right != NULL )
		csv_expr_free( x->right );
	if( x->list != NULL )
		csv_exprlist_free( x->list );
	if( (x->flags & EXPR_USE_PATTERN) && x->pat != NULL )
		g_dbe->regexp_free( x->pat );
	Safefree( x->token );
	Safefree( x->alias );
	Safefree( x->var.sv );
	Safefree( x );
}

INLINE void csv_exprlist_free( ExprList *l ) {
	size_t i;
	if( l == NULL )
		return;
#ifdef CSV_DEBUG
	_debug( "freeing exprlist 0x%x\n", l );
#endif
	for( i = 0; i < l->expr_count; i ++ )
		csv_expr_free( l->expr[i] );
	Safefree( l->expr );
	Safefree( l );
}

INLINE ExprList *csv_exprlist_add( ExprList *l, Expr *a, const Token *alias ) {
	if( l == NULL )
		Newxz( l, 1, ExprList );
	if( alias != NULL ) {
		if( alias->str[0] == '"' || alias->str[0] == '\'' ) {
			a->alias_len = alias->len - 2;
			Newx( a->alias, a->alias_len + 1, char );
			Copy( &alias->str[1], a->alias, a->alias_len, char );
			a->alias[a->alias_len] = '\0';
		}
		else {
			Newx( a->alias, alias->len + 1, char );
			Copy( alias->str, a->alias, alias->len, char );
			a->alias[alias->len] = '\0';
			a->alias_len = alias->len;
		}
	}
#ifdef CSV_DEBUG
	_debug( "exprlist 0x%x add expr 0x%x as [%s]\n", l, a, a->alias );
#endif
	if( (l->expr_count % 8) == 0 )
		Renew( l->expr, l->expr_count + 8, Expr * );
	l->expr[l->expr_count ++] = a;
	return l;
}

void csv_begin_parse( csv_parse_t *parse ) {
#ifdef CSV_DEBUG
	_debug( "text_begin_parse 0x%x\n", parse );
#endif
	parse->csv->last_errno = 0;
	parse->csv->affected_rows = 0;
	Safefree( parse->qmark );
	parse->qmark_count = 0;
	parse->type = parse->flags = 0;
	parse->table = NULL;
}

INLINE int expr_eval( csv_parse_t *parse, Expr *x ) {
	size_t i;
	int r;
	Expr *a, *b;
	if( x == NULL )
		return CSV_OK;
	if( x->list != NULL ) {
		for( i = 0; i < x->list->expr_count; i ++ ) {
			if( expr_eval( parse, x->list->expr[i] ) != CSV_OK )
				return CSV_ERROR;
		}
	}
	a = x->left;
	b = x->right;
	if( a != NULL ) {
		if( expr_eval( parse, a ) != CSV_OK )
			return CSV_ERROR;
	}
	if( b != NULL ) {
		if( expr_eval( parse, b ) != CSV_OK )
			return CSV_ERROR;
	}
	if( (x->flags & EXPR_IS_ID) && x->column != NULL ) {
		csv_table_def_t *table;
		char *p1, *p2;
		size_t j;
		if( x->flags & EXPR_USE_REF ) {
			VAR_COPY( x->ref->var, x->var );
			return CSV_OK;
		}
		table = x->column->table;
		p1 = &table->data[x->column->offset];
		i = x->column->length;
		if( i == 4 && strcmp( "NULL", p1 ) == 0 ) {
			x->var.flags = x->var.flags
				& (~(VAR_HAS_SV + VAR_HAS_NV + VAR_HAS_IV));
			return CSV_OK;
		}
		switch( x->column->type ) {
		case FIELD_TYPE_INTEGER:
			x->var.iv = atoi( p1 );
			VAR_FLAG_IV( x->var );
			return CSV_OK;
		case FIELD_TYPE_DOUBLE:
			if( table->decimal_symbol != '.' )
				if( (p2 = strchr( p1, table->decimal_symbol )) != NULL )
					*p2 = '.';
			x->var.nv = atof( p1 );
			VAR_FLAG_NV( x->var );
			return CSV_OK;
		case FIELD_TYPE_CHAR:
			x->var.sv_len = charset_convert(
				p1, i, table->charset,
				parse->csv->client_charset, &x->var.sv
			);
			VAR_FLAG_SV( x->var );
			return CSV_OK;
		case FIELD_TYPE_BLOB:
			if( i >= 2 && p1[0] == '0' && p1[1] == 'x' ) {
				i = (i - 2) / 2;
				if( x->var.sv_len < i )
					Renew( x->var.sv, i + 1, char );
				for( j = 0, p1 += 2; j < i; j ++ ) {
					r = CHAR_FROM_HEX[*p1 & 0x7f] << 4;
					p1 ++;
					r += CHAR_FROM_HEX[*p1 & 0x7f];
					p1 ++;
					x->var.sv[j] = (char) r;
				}
				x->var.sv[i] = '\0';
				x->var.sv_len = i;
				VAR_FLAG_SV( x->var );
				return CSV_OK;
			}
		default:
			if( x->var.sv_len < i )
				Renew( x->var.sv, i + 1, char );
			Copy( p1, x->var.sv, i, char );
			x->var.sv[i] = '\0';
			x->var.sv_len = i;
			VAR_FLAG_SV( x->var );
		}
		return CSV_OK;
	}
	switch( x->op ) {
	case TK_MINUS:
		VAR_EVAL_NUM( a->var );
		if( a->var.flags & VAR_HAS_IV ) {
			x->var.iv = - a->var.iv;
			VAR_FLAG_IV( x->var );
		}
		else {
			x->var.nv = - a->var.nv;
			VAR_FLAG_NV( x->var );
		}
		break;
	case TK_PLUS:
		VAR_EVAL_NUM( a->var );
		if( a->var.flags & VAR_HAS_IV ) {
			x->var.iv = + a->var.iv;
			VAR_FLAG_IV( x->var );
		}
		else {
			x->var.nv = + a->var.nv;
			VAR_FLAG_NV( x->var );
		}
		break;
	case TK_ADD:
		VAR_NUM_OP( x->var, a->var, b->var, +, "+" );
		break;
	case TK_SUB:
		VAR_NUM_OP( x->var, a->var, b->var, -, "-" );
		break;
	case TK_MUL:
		VAR_NUM_OP( x->var, a->var, b->var, *, "*" );
		break;
	case TK_DIV:
		VAR_NUM_OP( x->var, a->var, b->var, /, "/" );
		break;
	case TK_BITAND:
		VAR_EVAL_IV( a->var );
		VAR_EVAL_IV( b->var );
		x->var.iv = a->var.iv & b->var.iv;
		x->var.flags = VAR_HAS_IV;
		break;
	case TK_BITOR:
		VAR_EVAL_IV( a->var );
		VAR_EVAL_IV( b->var );
		x->var.iv = a->var.iv | b->var.iv;
		x->var.flags = VAR_HAS_IV;
		break;
	case TK_MOD:
		VAR_EVAL_IV( a->var );
		VAR_EVAL_IV( b->var );
		x->var.iv = a->var.iv % b->var.iv;
		x->var.flags = VAR_HAS_IV;
		break;
	case TK_EXP:
		VAR_EVAL_NUM( a->var );
		VAR_EVAL_NUM( b->var );
		if( a->var.flags & VAR_HAS_IV ) {
			if( b->var.flags & VAR_HAS_IV ) {
				x->var.iv = (long) pow( a->var.iv, b->var.iv );
				x->var.flags = VAR_HAS_IV;
			}
			else {
				x->var.nv = pow( a->var.iv, b->var.nv );
				x->var.flags = VAR_HAS_NV;
			}
		}
		else {
			if( b->var.flags & VAR_HAS_IV )
				x->var.nv = pow( a->var.nv, b->var.iv );
			else
				x->var.nv = pow( a->var.nv, b->var.nv );
			x->var.flags = VAR_HAS_NV;
		}
		break;
	case TK_SHL:
		VAR_EVAL_IV( a->var );
		VAR_EVAL_IV( b->var );
		x->var.iv = a->var.iv << b->var.iv;
		x->var.flags = VAR_HAS_IV;
		break;
	case TK_SHR:
		VAR_EVAL_IV( a->var );
		VAR_EVAL_IV( b->var );
		x->var.iv = a->var.iv >> b->var.iv;
		x->var.flags = VAR_HAS_IV;
		break;
	case TK_EQ:
		VAR_COMP_OP( x->var, a->var, b->var, ==, "==", 0, 0 );
		break;
	case TK_NE:
		VAR_COMP_OP( x->var, a->var, b->var, !=, "!=", 1, 1 );
		break;
	case TK_LE:
		VAR_COMP_OP( x->var, a->var, b->var, <=, "<=", 0, 1 );
		break;
	case TK_GE:
		VAR_COMP_OP( x->var, a->var, b->var, >=, ">=", 1, 0 );
		break;
	case TK_LT:
		VAR_COMP_OP( x->var, a->var, b->var, <, "<", 0, 1 );
		break;
	case TK_GT:
		VAR_COMP_OP( x->var, a->var, b->var, >, ">", 1, 0 );
		break;
	case TK_AND:
		if( a->var.flags & VAR_HAS_IV )
			x->var.iv = a->var.iv != 0;
		else if( a->var.flags & VAR_HAS_NV )
			x->var.iv = a->var.nv != 0;
		else
			x->var.iv = a->var.sv_len > 0 && a->var.sv[0] != '0';
		if( x->var.iv ) {
			if( b->var.flags & VAR_HAS_IV )
				x->var.iv = b->var.iv != 0;
			else if( b->var.flags & VAR_HAS_NV )
				x->var.iv = b->var.nv != 0;
			else
				x->var.iv = b->var.sv_len > 0 && b->var.sv[0] != '0';
		}
		VAR_FLAG_IV( x->var );
		break;
	case TK_OR:
		if( a->var.flags & VAR_HAS_IV )
			x->var.iv = a->var.iv != 0;
		else if( a->var.flags & VAR_HAS_NV )
			x->var.iv = a->var.nv != 0;
		else
			x->var.iv = a->var.sv_len > 0 && a->var.sv[0] != '0';
		if( ! x->var.iv ) {
			if( b->var.flags & VAR_HAS_IV )
				x->var.iv = b->var.iv != 0;
			else if( b->var.flags & VAR_HAS_NV )
				x->var.iv = b->var.nv != 0;
			else
				x->var.iv = b->var.sv_len > 0 && b->var.sv[0] != '0';
		}
		VAR_FLAG_IV( x->var );
		break;
	case TK_LIKE:
		VAR_EVAL_SV( a->var );
		x->var.iv = g_dbe->regexp_match( b->pat, a->var.sv, a->var.sv_len );
		break;
	case TK_NOTLIKE:
		VAR_EVAL_SV( a->var );
		x->var.iv = ! g_dbe->regexp_match( b->pat, a->var.sv, a->var.sv_len );
		break;
	case TK_NOT:
		if( a->var.flags & VAR_HAS_IV )
			x->var.iv = ! a->var.iv;
		else if( a->var.flags & VAR_HAS_NV )
			x->var.iv = ! a->var.nv;
		else
			x->var.iv = a->var.sv_len == 0 || a->var.sv[0] == '0';
		VAR_FLAG_IV( x->var );
		break;
	case TK_ISNULL:
		x->var.iv = (a->var.flags & (VAR_HAS_SV + VAR_HAS_NV + VAR_HAS_IV)) == 0;
		VAR_FLAG_IV( x->var );
		break;
	case TK_NOTNULL:
		x->var.iv = (a->var.flags & (VAR_HAS_SV + VAR_HAS_NV + VAR_HAS_IV)) != 0;
		VAR_FLAG_IV( x->var );
		break;
	case TK_BETWEEN:
		VAR_COMP_OP( x->var, x->left->var, x->list->expr[0]->var, >=, ">=", 1, 0 );
		if( x->var.iv )
			VAR_COMP_OP( x->var, x->left->var, x->list->expr[1]->var, <=, "<=", 1, 1 );
		break;
	case TK_IF:
		if( a->var.flags & VAR_HAS_IV )
			i = a->var.iv != 0;
		else if( a->var.flags & VAR_HAS_NV )
			i = a->var.nv != 0;
		else
			i = a->var.sv_len > 0 && a->var.sv[0] != '0';
		if( i )
			VAR_COPY( x->list->expr[0]->var, x->var )
		else
			VAR_COPY( x->list->expr[1]->var, x->var )
		break;
	case TK_COUNT:
		fnc_count( x );
		break;
	case TK_SUM:
		fnc_sum( x );
		break;
	case TK_AVG:
		fnc_avg( x );
		break;
	case TK_MIN:
		fnc_min( x );
		break;
	case TK_MAX:
		fnc_max( x );
		break;
	case TK_CONCAT:
		fnc_concat( x );
		break;
	case TK_ABS:
		fnc_abs( x );
		break;
	case TK_ROUND:
		fnc_round( x );
		break;
	case TK_CURRENT_TIME:
		fnc_current_time( x );
		break;
	case TK_CURRENT_DATE:
		fnc_current_date( x );
		break;
	case TK_CURRENT_TIMESTAMP:
		fnc_current_timestamp( x ); break;
	case TK_UPPER:
		fnc_upper( x, parse->csv->client_charset );
		break;
	case TK_LOWER:
		fnc_lower( x, parse->csv->client_charset );
		break;
	case TK_TRIM:
		fnc_trim( x );
		break;
	case TK_LTRIM:
		fnc_ltrim( x );
		break;
	case TK_RTRIM:
		fnc_rtrim( x );
		break;
	case TK_LENGTH:
		fnc_octet_length( x, parse->csv->client_charset );
		break;
	case TK_CHAR_LENGTH:
		fnc_char_length( x );
		break;
	case TK_SUBSTR:
		fnc_substr( x, parse->csv->client_charset );
		break;
	case TK_LOCATE:
		fnc_locate( x, parse->csv->client_charset );
		break;
	case TK_ASCII:
		fnc_ascii( x );
		break;
	case TK_CONVERT:
		if( fnc_convert( parse, x ) != CSV_OK )
			return CSV_ERROR;
		break;
	}
	return CSV_OK;
}

void csv_error_msg( csv_parse_t *parse, int code, const char *msg, ... ) {
	va_list a;
	va_start( a, msg );
	if( msg != NULL ) {
		vsnprintf(
			parse->csv->last_error, sizeof( parse->csv->last_error ), msg, a );
#ifdef CSV_DEBUG
		_debug( "Error (%d) %s\n", code, parse->csv->last_error );
#endif
	}
	else
		parse->csv->last_error[0] = '\0';
	va_end( a );
	parse->csv->last_errno = code;
}

void csv_error_msg2( csv_t *csv, int code, const char *msg, ... ) {
	va_list a;
	va_start( a, msg );
	if( msg != NULL ) {
		vsnprintf(
			csv->last_error, sizeof( csv->last_error ), msg, a );
#ifdef CSV_DEBUG
		_debug( "Error (%d) %s\n", code, csv->last_error );
#endif
	}
	else
		csv->last_error[0] = '\0';
	va_end( a );
	csv->last_errno = code;
}

int expr_bind_columns(
	csv_parse_t *parse, Expr *x, csv_table_def_t **tables, size_t table_count,
	const char *place
) {
	size_t i, j;
	if( x == NULL )
		return CSV_OK;
	if( x->op == TK_DOT ) {
		/* x->left=table, x->right=field */
		csv_table_def_t *table;
		csv_column_def_t *col;
		Expr *x1 = x->left, *x2 = x->right;
#ifdef CSV_DEBUG
		_debug( "bind column %s.%s\n", x1->var.sv, x2->var.sv );
#endif
		for( i = 0; i < table_count; i ++ ) {
			table = tables[i];
			if( my_stricmp( table->alias, x1->var.sv ) != 0 &&
				my_stricmp( table->name, x1->var.sv ) != 0
			) {
				continue;
			}
			if( x2->op == TK_ALL ) {
				/* expr defines table.* */
				x->table = table;
				x->op = TK_TABLE;
				return CSV_OK;
			}
			for( j = 0; j < table->column_count; j ++ ) {
				col = &table->columns[j];
				if( my_stricmp( col->name, x2->var.sv ) != 0 )
					continue;
				x->column = col;
				x->flags = EXPR_IS_ID;
				return CSV_OK;
			}
			csv_error_msg( parse, CSV_ERR_UNKNOWNCOLUMN,
				"Unknown column in %s: Table '%s' has no column '%s'",
				place, x1->var.sv, x2->var.sv
			);
			return CSV_ERROR;
		}
		csv_error_msg( parse, CSV_ERR_UNKNOWNTABLE,
			"Unknown table '%s' in %s", x1->var.sv, place );
		return CSV_ERROR;
	}
	if( x->left != NULL )
		if( CSV_OK != expr_bind_columns(
			parse, x->left, tables, table_count, place ) )
				return CSV_ERROR;
	if( x->right != NULL )
		if( CSV_OK != expr_bind_columns(
			parse, x->right, tables, table_count, place ) )
				return CSV_ERROR;
	if( x->list != NULL )
		for( i = 0; i < x->list->expr_count; i ++ )
			if( CSV_OK != expr_bind_columns(
				parse, x->list->expr[i], tables, table_count, place ) )
					return CSV_ERROR;
	if( x->flags & EXPR_IS_AGGR )
		parse->has_aggr = 1;
	else if( x->flags & EXPR_IS_QUEST ) {
#ifdef CSV_DEBUG
		_debug( "found question mark\n" );
#endif
		if( (parse->qmark_count % 4) == 0 )
			Renew( parse->qmark, parse->qmark_count + 4, Expr * );
		parse->qmark[parse->qmark_count ++] = x;
	}
	if( x->flags & EXPR_IS_ID ) {
		csv_table_def_t *table;
		csv_column_def_t *col;
#ifdef CSV_DEBUG
		_debug( "bind column %s\n", x->var.sv );
#endif
		for( i = 0; i < table_count; i ++ ) {
			table = tables[i];
			for( j = 0; j < table->column_count; j ++ ) {
				col = &table->columns[j];
				if( my_stricmp( x->var.sv, col->name ) == 0 ) {
					x->column = col;
					return CSV_OK;
				}
			}
		}
		csv_error_msg( parse, CSV_ERR_UNKNOWNCOLUMN,
			"Unknown column '%s' in %s", x->var.sv, place );
		return CSV_ERROR;
	}
	return CSV_OK;
}

#ifdef CSV_DEBUG

void print_expr( Expr *x, int level ) {
	int i;
	if( x == NULL ) {
		printf( "Expression: (null)\n" );
		return;
	}
	VAR_EVAL_SV( x->var );
	for( i = 0; i < level; i ++ )
		printf( "\t" );
	printf( "Expression: [0x%lx] sv[%s] flags[%u] op[%d] tk[%.*s]"
			" - left([0x%lx] sv[%s]) - right([0x%lx] sv[%s])\n",
		(size_t) x, x->var.sv, x->flags, x->op,
		x->token != NULL ? (int) x->token->len : 6,
		x->token != NULL ? x->token->str : "(null)",
		(size_t) x->left, x->left != NULL ? x->left->var.sv : NULL,
		(size_t) x->right, x->right != NULL ? x->right->var.sv : NULL
	);
	if( x->list )
		print_exprlist( x->list, level + 1 );
	if( x->left ) {
		print_expr( x->left, level + 1 );
	}
	if( x->right ) {
		print_expr( x->right, level + 1 );
	}
}

void print_exprlist( ExprList *l, int level ) {
	size_t i;
	int j;
	if( l == NULL ) {
		printf( "Expression List: (null)\n" );
		return;
	}
	for( j = 0; j < level; j ++ )
		printf( "\t" );
	printf( "Expression List [0x%lx] entries %lu\n",
		(size_t) l, l->expr_count );
	for( i = 0; i < l->expr_count; i ++ ) {
		for( j = 0; j < level + 1; j ++ )
			printf( "\t" );
		printf( "Expression %lu\n", i + 1 );
		print_expr( l->expr[i], level + 1 );
	}
}

#endif

void csv_column_def_free( csv_column_def_t *column ) {
	if( column == NULL )
		return;
	Safefree( column->name );
	Safefree( column->typename );
	Safefree( column->defval );
	Safefree( column->tablename );
}

void csv_result_free( csv_result_t *result ) {
	size_t i, j;
	csv_row_t *row;
	if( result == NULL )
		return;
	if( result->row_count ) {
		for( i = result->row_count - 1; ; i -- ) {
			row = result->rows[i];
			for( j = result->column_count - 1; ; j -- ) {
				Safefree( row->data[j].sv );
				if( j == 0 )
					break;
			}
			Safefree( row->data );
			Safefree( row );
			if( i == 0 )
				break;
		}
	}
	for( i = 0; i < result->column_count; i ++ )
		csv_column_def_free( &result->columns[i] );
	Safefree( result->columns );
	Safefree( result->rows );
	Safefree( result );
}

int csv_set_path( csv_t *csv, char *path, size_t path_len ) {
	DIR *dh;
	if( path != NULL ) {
		if( path_len == 0 )
			path_len = strlen( path );
		if( path_len > 1000 ) {
			csv_error_msg2( csv, CSV_ERR_INVALIDPATH,
				"Directory is to big (%s)", path );
			return CSV_ERROR;
		}
		if( (dh = PerlDir_open( path )) == NULL ) {
			csv_error_msg2( csv, CSV_ERR_INVALIDPATH,
				"Directory not found (%s)", path );
			return CSV_ERROR;
		}
		PerlDir_close( dh );
		Renew( csv->path, path_len + 2, char );
		Copy( path, csv->path, path_len, char );
		if( path[path_len - 1] != '\\' && path[path_len - 1] != '/' )
			csv->path[path_len ++] = DIRSEP;
		csv->path[path_len] = '\0';
		csv->path_length = path_len;
	}
	else {
		Safefree( csv->path );
		csv->path_length = 0;
	}
	return CSV_OK;
}
