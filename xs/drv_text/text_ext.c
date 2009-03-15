#include "text_ext.h"

csv_t *csv_init( csv_t *csv ) {
	if( csv == NULL ) {
		Newxz( csv, 1, csv_t );
		csv->flags = CSV_ISALLOC;
	}
	else {
		Zero( csv, 1, csv_t );
	}
	csv->backend = BACKEND_CSV;
	csv->flags |= CSV_COLNAMEHEADER | CSV_USESCHEMA;
	csv->quote = DEFAULT_QUOTECHAR;
	csv->delimiter = DEFAULT_DELIMITER;
	csv->max_scan_rows = DEFAULT_MAXSCANROWS;
	csv->decimal_symbol = '.';
	csv->table_charset = CS_ANSI;
	csv->client_charset = CS_ANSI;
	return csv;
}

void csv_free( csv_t *csv ) {
	if( csv == NULL )
		return;
	Safefree( csv->path );
	if( csv->flags & CSV_ISALLOC )
		Safefree( csv );
}

int csv_prepare( csv_t *csv, const char *sql, size_t sql_len, csv_stmt_t **pstmt ) {
	csv_stmt_t *stmt;
	Newxz( stmt, 1, csv_stmt_t );
	stmt->csv = csv;
	stmt->parse.csv = csv;
	stmt->parse.stmt = stmt;
	if( run_parser( &stmt->parse, sql, sql_len ) != 0 || csv->last_errno ) {
		csv_stmt_free( stmt );
		return CSV_ERROR;
	}
	(*pstmt) = stmt;
	return CSV_OK;
}

int csv_execute( csv_stmt_t *stmt ) {
	int r;
	if( stmt == NULL )
		return CSV_ERROR;
	switch( stmt->csv->backend ) {
	case BACKEND_CSV:
		switch( stmt->parse.type ) {
		case PARSE_TYPE_SELECT:
			if( stmt->result != NULL ) {
				csv_result_free( stmt->result );
				stmt->result = NULL;
			}
			stmt->result = csv_select_run( &stmt->parse );
			if( stmt->result == NULL )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_CREATE_TABLE:
			r = csv_create_table( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_DROP_TABLE:
			r = csv_drop_table( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_INSERT:
			r = csv_insert_run( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_UPDATE:
			r = csv_update_run( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_DELETE:
			r = csv_delete_run( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_SET:
			r = csv_set_run( &stmt->parse );
			if( r != CSV_OK )
				return CSV_ERROR;
			break;
		case PARSE_TYPE_SHOW_VARS:
			if( stmt->result != NULL ) {
				csv_result_free( stmt->result );
				stmt->result = NULL;
			}
			stmt->result = csv_run_show_variables( &stmt->parse );
			if( stmt->result == NULL )
				return CSV_ERROR;
			break;
		}
		break;
	}
	return CSV_OK;
}

void csv_stmt_free( csv_stmt_t *stmt ) {
	if( stmt == NULL )
		return;
	switch( stmt->csv->backend ) {
	case BACKEND_CSV:
		switch( stmt->parse.type ) {
		case PARSE_TYPE_SELECT:
			if( stmt->result != NULL )
				csv_result_free( stmt->result );
			csv_select_free( stmt->parse.select );
			break;
		case PARSE_TYPE_CREATE_TABLE:
			csv_table_def_free( stmt->parse.table );
			break;
		case PARSE_TYPE_DROP_TABLE:
			csv_table_def_free( stmt->parse.table );
			break;
		case PARSE_TYPE_INSERT:
			csv_insert_free( stmt->parse.insert );
			break;
		case PARSE_TYPE_UPDATE:
			csv_update_free( stmt->parse.update );
			break;
		case PARSE_TYPE_DELETE:
			csv_delete_free( stmt->parse.delete );
			break;
		case PARSE_TYPE_SET:
			csv_expr_free( stmt->parse.expr );
			break;
		case PARSE_TYPE_SHOW_VARS:
			csv_expr_free( stmt->parse.expr );
			break;
		}
		break;
	}
	Safefree( stmt->parse.qmark );
	Safefree( stmt->parse.sql );
	Safefree( stmt );
}

int csv_bind_param_sv(
	csv_stmt_t *stmt, WORD p_num, const char *val, size_t len
) {
	Expr *x;
	if( stmt == NULL )
		return CSV_OK;
	if( p_num < 1 || p_num > stmt->parse.qmark_count ) {
		csv_error_msg( &stmt->parse, CSV_ERR_BINDCOLUMN,
			"Parameter %u out of range %u", p_num, stmt->parse.qmark_count );
		return CSV_ERROR;
	}
	x = stmt->parse.qmark[p_num - 1];
	Renew( x->var.sv, len + 1, char );
	Copy( val, x->var.sv, len, char );
	x->var.sv[len] = '\0';
	x->var.sv_len = len;
	x->var.flags = (x->var.flags & (~(VAR_HAS_IV + VAR_HAS_NV))) | VAR_HAS_SV;
	if( x->flags & EXPR_USE_PATTERN ) {
		if( x->pat != NULL )
			g_dbe->regexp_free( x->pat );
		x->pat = g_dbe->regexp_from_pattern( x->var.sv, x->var.sv_len, 1 );
	}
	return CSV_OK;
}

int csv_bind_param_iv( csv_stmt_t *stmt, WORD p_num, int val ) {
	Expr *x;
	if( stmt == NULL )
		return CSV_OK;
	if( p_num < 1 || p_num > stmt->parse.qmark_count ) {
		csv_error_msg( &stmt->parse, CSV_ERR_BINDCOLUMN,
			"Parameter %u out of range %u", p_num, stmt->parse.qmark_count );
		return CSV_ERROR;
	}
	x = stmt->parse.qmark[p_num - 1];
	x->var.iv = val;
	x->var.flags = (x->var.flags & (~(VAR_HAS_SV + VAR_HAS_NV))) | VAR_HAS_IV;
	if( x->flags & EXPR_USE_PATTERN ) {
		char tmp[22], *p1;
		p1 = my_itoa( tmp, val, 10 );
		if( x->pat != NULL )
			g_dbe->regexp_free( x->pat );
		x->pat = g_dbe->regexp_from_pattern( tmp, p1 - tmp, 1 );
	}
	return CSV_OK;
}

int csv_bind_param_nv( csv_stmt_t *stmt, WORD p_num, double val ) {
	Expr *x;
	if( stmt == NULL )
		return CSV_OK;
	if( p_num < 1 || p_num > stmt->parse.qmark_count ) {
		csv_error_msg( &stmt->parse, CSV_ERR_BINDCOLUMN,
			"Parameter %u out of range %u", p_num, stmt->parse.qmark_count );
		return CSV_ERROR;
	}
	x = stmt->parse.qmark[p_num - 1];
	x->var.nv = val;
	x->var.flags = (x->var.flags & (~(VAR_HAS_SV + VAR_HAS_IV))) | VAR_HAS_NV;
	if( x->flags & EXPR_USE_PATTERN ) {
		char tmp[41], *p1;
		p1 = my_ftoa( tmp, val );
		if( x->pat != NULL )
			g_dbe->regexp_free( x->pat );
		x->pat = g_dbe->regexp_from_pattern( tmp, p1 - tmp, 1 );
	}
	return CSV_OK;
}

csv_result_t *csv_get_result( csv_stmt_t *stmt ) {
	csv_result_t *result = stmt->result;
	stmt->result = NULL;
	return result;
}
