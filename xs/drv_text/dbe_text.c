#include "dbe_text.h"

void dbe_drv_free( drv_def_t *dbe_drv ) {
#if CSV_DEBUG > 1
	debug_free();
#endif
}

void dbe_mem_free( void *ptr ) {
	Safefree( ptr );
}

drv_con_t *dbe_drv_connect( dbe_con_t *dbe_con, char **args, int argc ) {
	int i, j;
	size_t l;
	drv_con_t *con;
	csv_t *csv;
	char *key, *val, *s1;
	dbe_str_t *extensions = NULL;
	Newxz( con, 1, drv_con_t );
	csv = csv_init( &con->csv );
	for( i = 0; i < argc; i += 2 ) {
#ifdef CSV_DEBUG
		_debug( "argument %d [%s] => %s\n", i / 2 + 1, args[i], args[i + 1] );
#endif
		key = args[i];
		val = args[i + 1];
		if( my_stricmp( "DBQ", key ) == 0 ||
			my_stricmp( "DEFAULTDIR", key ) == 0
		) {
			if( CSV_OK != csv_set_path( csv, val, 0 ) ) {
				g_dbe->set_error(
					dbe_con, csv->last_errno, csv->last_error );
				goto error;
			}
		}
		else if( my_stricmp( "USESCHEMA", key ) == 0 ) {
			if( my_stricmp( "FALSE", val ) == 0 ||
				my_stricmp( "NO", val ) == 0 ||
				*val == '0'
			)
				csv->flags &= (~CSV_USESCHEMA);
			else
				csv->flags |= CSV_USESCHEMA;
		}
		else if( my_stricmp( "COLNAMEHEADER", key ) == 0 ) {
			if( *val == '0' ||
				my_stricmp( "FALSE", val ) == 0 ||
				my_stricmp( "NO", val ) == 0
			)
				csv->flags &= (~CSV_COLNAMEHEADER);
			else
				csv->flags |= CSV_COLNAMEHEADER;
		}
		else if( my_stricmp( "QUOTECHAR", key ) == 0 ) {
			if( *val != '0' )
				csv->quote = *val;
		}
		else if( my_stricmp( "DECIMALSYMBOL", key ) == 0 ||
			my_stricmp( "DEC", key ) == 0
		) {
			if( *val != '0' )
				csv->decimal_symbol = *val;
		}
		else if( my_stricmp( "FORMAT", key ) == 0 ||
			my_stricmp( "FMT", key ) == 0
		) {
			if( (val = strchr( val, '(' )) != NULL )
				csv->delimiter = *(val + 1);
			else if( my_stricmp( "CSVDELIMITED", val ) == 0 )
				csv->delimiter = ',';
			else if( my_stricmp( "TABDELIMITED", val ) == 0 )
				csv->delimiter = '\t';
		}
		else if( my_stricmp( "DELIMITER", key ) == 0 ) {
			if( *val != '0' )
				csv->delimiter = *val;
		}
		else if( my_stricmp( "MAXSCANROWS", key ) == 0 ||
			my_stricmp( "MSR", key ) == 0
		) {
			csv->max_scan_rows = atoi( val );
			if( csv->max_scan_rows < 1 )
				csv->max_scan_rows = 1;
		}
		else if( my_stricmp( "EXTENSIONS", key ) == 0 ) {
			j = 0;
			con->extensions_length = 0;
			for( j = 0; *val != '\0'; ) {
				for( ; isSPACE( *val ); val ++ );
				for( s1 = val; *s1 != ',' && *s1 != '\0'; s1 ++ );
				for( ; isSPACE( *s1 ); s1 -- );
				if( s1 > val ) {
					if( (j % 4) == 0 )
						Renew( extensions, 4, dbe_str_t );
					extensions[j].length = l = s1 - val;
					con->extensions_length += l;
					Newx( extensions[j].value, l + 1, char );
					Copy( val, extensions[j].value, l, char );
					extensions[j].value[l] = '\0';
#ifdef CSV_DEBUG
					_debug( "found extension [%s]\n", extensions[j].value );
#endif
					j ++;
				}
				if( *s1 == '\0' )
					break;
				val = s1 + 1;
			}
			con->extensions = extensions;
			con->extensions_count = j;
		}
		else if( my_stricmp( "CHARSET", key ) == 0 ||
			my_stricmp( "CHARACTERSET", key ) == 0
		) {
			enum n_charset csid = get_charset_id( val );
			if( csid == CS_UNKNOWN ) {
				g_dbe->set_error( con->pcon, CSV_ERR_UNKNOWNCHARSET,
					"Unknown character set '%s'", val );
				goto error;
			}
			con->csv.client_charset = csid;
		}
	}
	con->pcon = dbe_con;
	if( extensions == NULL ) {
		Newx( extensions, 2, dbe_str_t );
		extensions[0].length = 3;
		Newx( extensions[0].value, 4, char );
		my_strcpy( extensions[0].value, "csv" );
		extensions[1].length = 3;
		Newx( extensions[1].value, 4, char );
		my_strcpy( extensions[1].value, "txt" );
		con->extensions = extensions;
		con->extensions_count = 2;
		con->extensions_length = 6;
	}
	return con;
error:
	Safefree( con );
	return NULL;
}

int dbe_con_reconnect( drv_con_t *drv_con ) {
	return DBE_OK;
}

int dbe_con_ping( drv_con_t *drv_con ) {
	return DBE_OK;
}

void dbe_con_free( drv_con_t *con ) {
	int i;
#ifdef CSV_DEBUG
	_debug( "dbe_con_free 0x%x called\n", (size_t) con );
#endif
	csv_free( &con->csv );
	for( i = 0; i < con->extensions_count; i ++ )
		Safefree( con->extensions[i].value );
	Safefree( con->extensions );
	Safefree( con );
}

int dbe_con_set_attr( drv_con_t *con, const char *name, SV *value ) {
	char *str;
	STRLEN len;
	switch( toupper( *name ) ) {
	case 'C':
		if( my_stricmp( "CHARSET", name ) == 0 ||
			my_stricmp( "CHARACTERSET", name ) == 0
		) {
			enum n_charset csid;
			str = SvPV( value, len );
			csid = get_charset_id( str );
			if( csid == CS_UNKNOWN ) {
				g_dbe->set_error( con->pcon,
					CSV_ERR_UNKNOWNCHARSET, "Unknown character set '%s'", str );
				return DBE_ERROR;
			}
			con->csv.client_charset = csid;
			return DBE_OK;
		}
		break;
	}
	return DBE_INVALIDARG;
}

int dbe_con_get_attr( drv_con_t *con, const char *name, SV **rval ) {
	switch( toupper( *name ) ) {
	case 'C':
		if( my_stricmp( "CHARSET", name ) == 0 ||
			my_stricmp( "CHARACTERSET", name ) == 0
		) {
			*rval = newSVpv( get_charset_name( con->csv.client_charset ), 0 );
			return DBE_OK;
		}
		break;
	}
	return DBE_INVALIDARG;
}

int dbe_con_query(
	drv_con_t *con, const char *sql, size_t sql_len, dbe_res_t **dbe_res
) {
	drv_res_t *res;
	csv_stmt_t *stmt;
	csv_result_t *result;
	if( CSV_OK != csv_prepare( &con->csv, sql, sql_len, &stmt ) ) {
		g_dbe->set_error(
			con->pcon, con->csv.last_errno, con->csv.last_error );
		return DBE_ERROR;
	}
	if( CSV_OK != csv_execute( stmt ) ) {
		csv_stmt_free( stmt );
		g_dbe->set_error(
			con->pcon, con->csv.last_errno, con->csv.last_error );
		return DBE_ERROR;
	}
	result = csv_get_result( stmt );
	if( result != NULL ) {
		Newxz( res, 1, drv_res_t );
		res->con = con;
		res->res = result;
		(*dbe_res) = g_dbe->alloc_result(
			con->pcon, res, (u_long) result->column_count );
	}
	csv_stmt_free( stmt );
	return DBE_OK;
}

xlong dbe_con_affected_rows( drv_con_t *con ) {
	return (xlong) con->csv.affected_rows;
}

int dbe_con_prepare(
	drv_con_t *con, const char *sql, size_t sql_len, drv_stmt_t **drv_stmt
) {
	drv_stmt_t *stmt;
	csv_stmt_t *csv_stmt;
	if( CSV_OK != csv_prepare( &con->csv, sql, sql_len, &csv_stmt ) ) {
		g_dbe->set_error(
			con->pcon, con->csv.last_errno, con->csv.last_error );
		return DBE_ERROR;
	}
	Newxz( stmt, 1, drv_stmt_t );
	stmt->stmt = csv_stmt;
	stmt->con = con;
	(*drv_stmt) = stmt;
	return DBE_OK;
}

void dbe_stmt_free( drv_stmt_t *stmt ) {
	csv_stmt_free( stmt->stmt );
	Safefree( stmt );
}

int dbe_stmt_param_count( drv_stmt_t *stmt ) {
	return (int) stmt->stmt->parse.qmark_count;
}

int dbe_stmt_bind_param( drv_stmt_t *stmt, u_long p_num, SV *val, char type ) {
	int rv;
	char *s;
	STRLEN l;
	switch( type ) {
	case 'i':
		rv = csv_bind_param_iv( stmt->stmt, (WORD) p_num, (int) SvIV( val ) );
		goto check_rv;
	case 'd':
		rv = csv_bind_param_nv( stmt->stmt, (WORD) p_num, (double) SvNV( val ) );
		goto check_rv;
	case 's':
	case 'b':
		s = SvPV( val, l );
		rv = csv_bind_param_sv( stmt->stmt, (WORD) p_num, s, l );
		goto check_rv;
	}
	// auto detect
	if( SvIOK( val ) ) {
		rv = csv_bind_param_iv( stmt->stmt, (WORD) p_num, (int) SvIV( val ) );
	}
	else if( SvNOK( val ) ) {
		rv = csv_bind_param_nv( stmt->stmt, (WORD) p_num, (double) SvNV( val ) );
	}
	else {
		s = SvPV( val, l );
		rv = csv_bind_param_sv( stmt->stmt, (WORD) p_num, s, l );
	}
check_rv:
	return rv == CSV_OK ? DBE_OK : DBE_ERROR;
}

int dbe_stmt_execute( drv_stmt_t *stmt, dbe_res_t **dbe_res ) {
	drv_res_t *res;
	drv_con_t *con = stmt->con;
	csv_result_t *result;
	if( CSV_OK != csv_execute( stmt->stmt ) ) {
		g_dbe->set_error(
			con->pcon, con->csv.last_errno, con->csv.last_error );
		return DBE_ERROR;
	}
	result = csv_get_result( stmt->stmt );
	if( result != NULL ) {
		Newxz( res, 1, drv_res_t );
		res->con = con;
		res->res = result;
		(*dbe_res) = g_dbe->alloc_result(
			con->pcon, res, (u_long) result->column_count );
	}
	return DBE_OK;
}

void dbe_res_free( drv_res_t *res ) {
	csv_result_free( res->res );
	Safefree( res->fields );
	Safefree( res );
}

xlong dbe_res_num_rows( drv_res_t *res ) {
	return (xlong) res->res->row_count;
}

int dbe_res_fetch_names( drv_res_t *res, dbe_str_t *names, int *flags ) {
	csv_result_t *result = res->res;
	size_t i;
	for( i = 0; i < result->column_count; i ++ ) {
		names[i].value = result->columns[i].name;
		names[i].length = result->columns[i].name_length;
	}
	return DBE_OK;
}

int dbe_res_fetch_row( drv_res_t *res, SV **prow, int names ) {
	csv_result_t *result = res->res;
	csv_row_t *row;
	size_t i;
	csv_var_t *v;
	if( result->row_pos >= result->row_count )
		return DBE_NO_DATA;
	row = result->rows[result->row_pos ++];
	for( i = 0; i < result->column_count; i ++ ) {
		v = &row->data[i];
		if( v->flags & VAR_HAS_IV )
			prow[i] = newSViv( v->iv );
		else if( v->flags & VAR_HAS_NV )
			prow[i] = newSVnv( v->nv );
		else if( v->flags & VAR_HAS_SV )
			prow[i] = newSVpvn( v->sv, v->sv_len );
		else
			prow[i] = NULL;
	}
	return DBE_OK;
}

xlong dbe_res_row_seek( drv_res_t *res, u_xlong offset ) {
	csv_result_t *result = res->res;
	xlong lrp = result->row_pos;
	if( offset >= result->row_count )
		result->row_pos = result->row_count - 1;
	else
		result->row_pos = (u_long) offset;
	return lrp;
}

xlong dbe_res_row_tell( drv_res_t *res ) {
	return (xlong) res->res->row_pos;
}

#define FIELD_ITEM_COUNT 7 * 2

int dbe_res_fetch_field( drv_res_t *res, SV ***sv, int *flags ) {
	csv_result_t *result = res->res;
	csv_column_def_t *col;
	SV **args;
	if( result->column_pos >= result->column_count )
		return 0;
	col = result->columns + result->column_pos ++;
	if( res->fields == NULL )
		Newx( res->fields, FIELD_ITEM_COUNT, SV * );
	args = res->fields;
	args[0] = newSVpvn( "COLUMN_NAME", 11 );
	args[1] = newSVpvn( col->name, col->name_length );
	args[2] = newSVpvn( "TABLE_NAME", 10 );
	args[3] = col->tablename_length
		? newSVpvn( col->tablename, col->tablename_length ) : NULL;
	args[4] = newSVpvn( "COLUMN_SIZE", 11 );
	args[5] = newSVuv( col->size );
	args[6] = newSVpvn( "COLUMN_DEF", 10 );
	args[7] = newSVpvn( col->defval, col->defval_length );
	args[8] = newSVpvn( "DATA_TYPE", 9 );
	args[10] = newSVpvn( "TYPE_NAME", 9 );
	switch( col->type ) {
	case FIELD_TYPE_INTEGER:
		args[9] = newSViv( SQL_INTEGER );
		args[11] = newSVpvn( "INTEGER", 7 );
		break;
	case FIELD_TYPE_DOUBLE:
		args[9] = newSViv( SQL_DOUBLE );
		args[11] = newSVpvn( "DOUBLE", 6 );
		break;
	case FIELD_TYPE_BLOB:
		args[9] = newSViv( SQL_BLOB );
		args[11] = newSVpvn( "BLOB", 4 );
		break;
	case FIELD_TYPE_CHAR:
	default:
		args[9] = newSViv( SQL_CHAR );
		args[11] = newSVpvn( "CHAR", 4 );
		break;
	}
	args[12] = newSVpvn( "NULLABLE", 8 );
	args[13] = newSViv( 1 );
	(*sv) = args;
	//(*flags) = DBE_FREE_ARRAY;
	return FIELD_ITEM_COUNT;
}

u_long dbe_res_field_seek( drv_res_t *res, u_long offset ) {
	csv_result_t *result = res->res;
	u_long lcp = result->column_pos;
	if( offset > result->column_count )
		result->column_pos = result->column_count;
	else
		result->column_pos = offset;
	return lcp;
}

u_long dbe_res_field_tell( drv_res_t *res ) {
	return res->res->column_pos;
}

SV *dbe_con_quote( drv_con_t *con, const char *str, size_t str_len ) {
	return charset_quote( con->csv.client_charset, str, str_len );
}

SV *dbe_con_quote_bin(
	drv_con_t *con, const char *bin, size_t bin_len
) {
	char *tmp, *s1;
	const unsigned char *s2, *s4;
	SV *ret;
	Newx( tmp, bin_len * 2 + 2, char );
	s1 = tmp, *s1 ++ = '0', *s1 ++ = 'x';
	for( s2 = (unsigned char *) bin, s4 = s2 + bin_len; s2 < s4; s2 ++ ) {
		*s1 ++ = HEX_FROM_CHAR[*s2 / 16];
		*s1 ++ = HEX_FROM_CHAR[*s2 % 16];
	}
	ret = newSVpvn( tmp, s1 - tmp );
	Safefree( tmp );
	return ret;
}

SV *dbe_con_quote_id( drv_con_t *con, const char **args, int argc ) {
	return charset_quote_id( con->csv.client_charset, args, argc );
}

int dbe_con_getinfo (
	drv_con_t *con, int type, void *val, int cbvalmax, int *pcbval,
	enum dbe_getinfo_type *rtype
) {
	switch( type ) {
	case SQL_IDENTIFIER_QUOTE_CHAR:
		*rtype = DBE_GETINFO_STRING, *pcbval = 1;
		*((char *) val) = con->csv.quote, *((char *) val + 1) = '\0';
		break;
	case SQL_DRIVER_NAME:
		*pcbval = (int) strlen( DRV_NAME );
		*rtype = DBE_GETINFO_STRING;
		if( cbvalmax <= *pcbval )
			return DBE_GETINFO_TRUNCATE;
		my_strcpy( (char *) val, DRV_NAME );
		break;
	case SQL_DRIVER_VER:
	case SQL_DBMS_VER:
		*pcbval = (int) strlen( XS_VERSION );
		*rtype = DBE_GETINFO_STRING;
		if( cbvalmax <= *pcbval )
			return DBE_GETINFO_TRUNCATE;
		my_strcpy( (char *) val, XS_VERSION );
		break;
	case SQL_SERVER_NAME:
		*pcbval = (int) strlen( g_drv_def.description );
		*rtype = DBE_GETINFO_STRING;
		if( cbvalmax <= *pcbval )
			return DBE_GETINFO_TRUNCATE;
		my_strcpy( (char *) val, g_drv_def.description );
		break;
	case SQL_DBMS_NAME:
		*rtype = DBE_GETINFO_STRING, *pcbval = 4;
		my_strcpy( (char *) val, "Text" );
		break;
	case SQL_AGGREGATE_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_AF_AVG | SQL_AF_COUNT | SQL_AF_MAX | SQL_AF_MIN | SQL_AF_SUM;
		break;
	case SQL_NUMERIC_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_FN_NUM_ABS | SQL_FN_NUM_ROUND;
		break;
	case SQL_KEYWORDS:
	case SQL_PROCEDURE_TERM:
	case SQL_SCHEMA_TERM:
	case SQL_SPECIAL_CHARACTERS:
	case SQL_USER_NAME:
		*rtype = DBE_GETINFO_STRING, *pcbval = 0;
		*((char *) val) = '\0';
		break;
	case SQL_COLUMN_ALIAS:
	case SQL_EXPRESSIONS_IN_ORDERBY:
	case SQL_LIKE_ESCAPE_CLAUSE:
	case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
	case SQL_ACCESSIBLE_TABLES:
		*rtype = DBE_GETINFO_STRING, *pcbval = 1;
		*((char *) val) = 'Y', *((char *) val + 1) = '\0';
		break;
	case SQL_CATALOG_NAME:
	case SQL_DESCRIBE_PARAMETER:
	case SQL_MULT_RESULT_SETS:
	case SQL_NEED_LONG_DATA_LEN:
	case SQL_ORDER_BY_COLUMNS_IN_SELECT:
	case SQL_PROCEDURES:
	case SQL_ACCESSIBLE_PROCEDURES:
		*rtype = DBE_GETINFO_STRING, *pcbval = 1;
		*((char *) val) = 'N', *((char *) val + 1) = '\0';
		break;
	case SQL_ALTER_DOMAIN:
	case SQL_ALTER_TABLE:
	case SQL_CREATE_SCHEMA:
	case SQL_CREATE_VIEW:
	case SQL_DROP_SCHEMA:
	case SQL_DROP_VIEW:
	case SQL_DDL_INDEX:
	case SQL_INDEX_KEYWORDS:
	case SQL_MAX_CHAR_LITERAL_LEN:
	case SQL_MAX_INDEX_SIZE:
	case SQL_MAX_ROW_SIZE:
	case SQL_MAX_STATEMENT_LEN:
	case SQL_SCHEMA_USAGE:
	case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
	case SQL_SQL92_GRANT:
	case SQL_SUBQUERIES:
	case SQL_SYSTEM_FUNCTIONS:
	case SQL_TXN_ISOLATION_OPTION:
	case SQL_UNION:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = 0;
		break;
	case SQL_CURSOR_COMMIT_BEHAVIOR:
	case SQL_CURSOR_ROLLBACK_BEHAVIOR:
	case SQL_MAX_CATALOG_NAME_LEN:
	case SQL_MAX_COLUMN_NAME_LEN:
	case SQL_MAX_COLUMNS_IN_GROUP_BY:
	case SQL_MAX_COLUMNS_IN_INDEX:
	case SQL_MAX_COLUMNS_IN_ORDER_BY:
	case SQL_MAX_COLUMNS_IN_SELECT:
	case SQL_MAX_COLUMNS_IN_TABLE:
	case SQL_MAX_CURSOR_NAME_LEN:
	case SQL_MAX_IDENTIFIER_LEN:
	case SQL_MAX_SCHEMA_NAME_LEN:
	case SQL_MAX_TABLE_NAME_LEN:
	case SQL_MAX_TABLES_IN_SELECT:
	case SQL_MAX_USER_NAME_LEN:
	case SQL_TXN_CAPABLE:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = 0;
		break;
	case SQL_CATALOG_LOCATION:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_CL_START;
		break;
	case SQL_CATALOG_NAME_SEPARATOR:
		*rtype = DBE_GETINFO_STRING, *pcbval = 1;
		*((char *) val) = '/', *((char *) val + 1) = '\0';
		break;
	case SQL_CATALOG_TERM:
		*pcbval = 9;
		*rtype = DBE_GETINFO_STRING;
		my_strcpy( (char *) val, "directory" );
		break;
	case SQL_CATALOG_USAGE:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_CU_DML_STATEMENTS | SQL_CU_TABLE_DEFINITION;
		break;
	case SQL_CONCAT_NULL_BEHAVIOR:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_CB_NULL;
		break;
	case SQL_CONVERT_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_FN_CVT_CONVERT;
		break;
	case SQL_CORRELATION_NAME:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_CN_ANY;
		break;
	case SQL_CREATE_TABLE:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_CT_CREATE_TABLE | SQL_CT_COLUMN_DEFAULT;
		break;
	case SQL_DROP_TABLE:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_DT_DROP_TABLE;
		break;
	case SQL_DATA_SOURCE_NAME:
		*pcbval = (int) con->csv.path_length;
		*rtype = DBE_GETINFO_STRING;
		if( cbvalmax <= *pcbval )
			return DBE_GETINFO_TRUNCATE;
		my_strcpy( (char *) val, con->csv.path );
		break;
	case SQL_GROUP_BY:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_GB_NO_RELATION;
		break;
	case SQL_IDENTIFIER_CASE:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_IC_MIXED;
		break;
	case SQL_INSERT_STATEMENT:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_IS_INSERT_LITERALS | SQL_IS_INSERT_SEARCHED;
		break;
	case SQL_NON_NULLABLE_COLUMNS:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_NNC_NON_NULL;
		break;
	case SQL_NULL_COLLATION:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_NC_LOW;
		break;
	case SQL_OJ_CAPABILITIES:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = 0;
		break;
	case SQL_QUOTED_IDENTIFIER_CASE:
		*rtype = DBE_GETINFO_SHORTINT, *pcbval = 2;
		*((short *) val) = SQL_IC_SENSITIVE;
		break;
	case SQL_SEARCH_PATTERN_ESCAPE:
		*rtype = DBE_GETINFO_STRING, *pcbval = 1;
		*((char *) val) = '\\', *((char *) val + 1) = '\0';
		break;
	case SQL_SQL_CONFORMANCE:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_SC_SQL92_ENTRY;
		break;
	case SQL_SQL92_DATETIME_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME |
			SQL_SDF_CURRENT_TIMESTAMP;
		break;
	case SQL_SQL92_PREDICATES:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_SP_EXISTS | SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_LIKE |
			SQL_SP_BETWEEN | SQL_SP_COMPARISON;
		break;
	case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = 0;
		break;
	case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) = SQL_SRVC_VALUE_EXPRESSION | SQL_SRVC_NULL;
		break;
	case SQL_SQL92_STRING_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_SSF_LOWER | SQL_SSF_UPPER | SQL_SSF_SUBSTRING |
			SQL_SSF_TRIM_BOTH | SQL_SSF_TRIM_LEADING | SQL_SSF_TRIM_TRAILING;
		break;
	case SQL_STRING_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_FN_STR_CONCAT | SQL_FN_STR_LTRIM | SQL_FN_STR_LENGTH |
			SQL_FN_STR_LOCATE | SQL_FN_STR_LCASE | SQL_FN_STR_RTRIM |
			SQL_FN_STR_SUBSTRING | SQL_FN_STR_UCASE | SQL_FN_STR_ASCII |
			SQL_FN_STR_LOCATE_2 | SQL_FN_STR_CHAR_LENGTH |
			SQL_FN_STR_CHARACTER_LENGTH | SQL_FN_STR_OCTET_LENGTH |
			SQL_FN_STR_POSITION;
		break;
	case SQL_TABLE_TERM:
		*rtype = DBE_GETINFO_STRING, *pcbval = 4;
		my_strcpy( (char *) val, "file" );
		break;
	case SQL_TIMEDATE_FUNCTIONS:
		*rtype = DBE_GETINFO_32BITMASK, *pcbval = 4;
		*((unsigned int *) val) =
			SQL_FN_TD_NOW | SQL_FN_TD_CURRENT_DATE | SQL_FN_TD_CURRENT_TIME |
			SQL_FN_TD_CURRENT_TIMESTAMP;
		break;
	default:
		return DBE_GETINFO_INVALIDARG;
	}
	return DBE_OK;
}

#define SET_FIELD(str,sdata,slen) \
	str.value = (char *) (sdata), str.length = (size_t) (slen)

int dbe_con_type_info( drv_con_t *con, int type, dbe_res_t **dbe_res ) {
	dbe_res_t *vres;
	int type_count, i, data_type, col_size, case_sens, searchable, mins, maxs;
	int radix, nullable, typid;
	const int *types;
	const int d_types[] = { SQL_BLOB, SQL_DOUBLE, SQL_INTEGER, SQL_VARCHAR };
	dbe_str_t data[19];
	Zero( data, 19, dbe_str_t );
	vres = g_dbe->vres_create( con->pcon );
	g_dbe->vres_add_column( vres, "TYPE_NAME", 9, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( vres, "DATA_TYPE", 9, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "COLUMN_SIZE", 11, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "LITERAL_PREFIX", 14, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( vres, "LITERAL_SUFFIX", 14, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( vres, "CREATE_PARAMS", 13, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( vres, "NULLABLE", 8, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "CASE_SENSITIVE", 14, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "SEARCHABLE", 10, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "UNSIGNED_ATTRIBUTE", 18, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "FIXED_PREC_SCALE", 16, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "AUTO_UNIQUE_VALUE", 17, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "LOCAL_TYPE_NAME", 15, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( vres, "MINIMUM_SCALE", 13, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "MAXIMUM_SCALE", 13, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "SQL_DATATYPE", 12, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "SQL_DATETIME_SUB", 16, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "NUM_PREC_RADIX", 14, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( vres, "INTERVAL_PRECISION", 18, DBE_TYPE_INTEGER );
	if( type == SQL_ALL_TYPES )
		types = d_types, type_count = 4;
	else
		typid = (int) type, types = &typid, type_count = 1;
	data[1].value = (char *) &data_type;
	data[2].value = (char *) &col_size;
	data[6].value = (char *) &nullable, nullable = 1;
	data[7].value = (char *) &case_sens;
	data[8].value = (char *) &searchable, searchable = SQL_SEARCHABLE;
	data[15].value = data[1].value;
	for( i = 0; i < type_count; i ++ ) {
		switch( types[i] ) {
		case SQL_TINYINT:
		case SQL_SMALLINT:
		case SQL_INTEGER:
		case SQL_BIGINT:
			SET_FIELD( data[0], "INTEGER", 7 );
			data_type = SQL_INTEGER;
			col_size = 20;
			SET_FIELD( data[3], NULL, 0 );
			SET_FIELD( data[4], NULL, 0 );
			case_sens = 0;
			data[13].value = data[14].value = NULL;
			data[17].value = (char *) &radix, radix = 10;
			break;
		case SQL_FLOAT:
		case SQL_REAL:
		case SQL_DOUBLE:
		case SQL_NUMERIC:
		case SQL_DECIMAL:
			SET_FIELD( data[0], "DOUBLE", 6 );
			data_type = SQL_DOUBLE;
			col_size = 17;
			SET_FIELD( data[3], NULL, 0 );
			SET_FIELD( data[4], NULL, 0 );
			case_sens = 0;
			data[13].value = (char *) &mins, mins = 0;
			data[14].value = (char *) &maxs, maxs = 15;
			data[17].value = (char *) &radix, radix = 2;
			break;
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_CLOB:
			SET_FIELD( data[0], "TEXT", 4 );
			data_type = SQL_VARCHAR;
			col_size = 0x7fffffff;
			SET_FIELD( data[3], "'", 1 );
			SET_FIELD( data[4], "'", 1 );
			case_sens = 1;
			data[17].value = data[14].value = data[13].value = NULL;
			break;
		case SQL_BLOB:
		case SQL_BINARY:
		case SQL_VARBINARY:
		default:
			SET_FIELD( data[0], "BLOB", 4 );
			data_type = SQL_VARBINARY;
			col_size = 0x7fffffff;
			SET_FIELD( data[3], "0x", 2 );
			SET_FIELD( data[4], NULL, 0 );
			case_sens = 1;
			data[17].value = data[14].value = data[13].value = NULL;
			break;
		}
		g_dbe->vres_add_row( vres, data );
	}
	(*dbe_res) = vres;
	return DBE_OK;
}

int dbe_con_tables(
	drv_con_t *con, const char *catalog, size_t catalog_len, const char *schema,
	size_t schema_len, const char *table, size_t table_len, const char *type,
	size_t type_len, dbe_res_t **dbe_res
) {
	regexp *re_tb = NULL;
	char tmp[1024], *s1;
	DIR *dh;
	Direntry_t *de;
	dbe_res_t *pres = NULL;
	dbe_str_t data[5];
	size_t len;
	int r;
	if( table_len > 0 )
		re_tb = g_dbe->regexp_from_pattern( table, table_len, TRUE );
	else {
		s1 = my_strcpy( tmp, "/\\.(" );
		for( r = 0; r < con->extensions_count; r ++ ) {
			s1 = my_strcpy( s1, con->extensions[r].value );
			*s1 ++ = '|';
		}
		s1 = my_strcpy( s1 - 1, ")$/i" );
		re_tb = g_dbe->regexp_from_string( tmp, s1 - tmp );
	}
	if( con->csv.path != NULL )
		s1 = my_strcpy( tmp, con->csv.path );
	else
		s1 = my_strcpy( tmp, "." );
	if( (dh = PerlDir_open( tmp )) == NULL ) {
		g_dbe->set_error( con->pcon,
			-1, "Unable to open directory '%s'", tmp );
		return DBE_ERROR;
	}
	pres = g_dbe->vres_create( con->pcon );
	g_dbe->vres_add_column( pres, "TABLE_CAT", 9, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "TABLE_SCHEM", 11, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "TABLE_NAME", 10, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "TABLE_TYPE", 10, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "REMARKS", 7, DBE_TYPE_VARCHAR );
	SET_FIELD( data[0], NULL, 0 );
	SET_FIELD( data[1], NULL, 0 );
	SET_FIELD( data[3], "TABLE", 5 );
	SET_FIELD( data[4], NULL, 0 );
	while( (de = PerlDir_read( dh )) != NULL ) {
		if( de->d_name[0] == '.' )
			continue;
		len = strlen( de->d_name );
		if( ! g_dbe->regexp_match( re_tb, de->d_name, len ) )
			continue;
#ifdef CSV_DEBUG
		_debug( "entry %s\n", de->d_name );
#endif
		SET_FIELD( data[2], de->d_name, len );
		if( g_dbe->vres_add_row( pres, data ) != DBE_OK )
			goto error;
	}
	(*dbe_res) = pres;
	r = DBE_OK;
	goto exit;
error:
	r = DBE_ERROR;
exit:
	PerlDir_close( dh );
	g_dbe->regexp_free( re_tb );
	return r;
}

int dbe_con_columns(
	drv_con_t *con, const char *catalog, size_t catalog_len, const char *schema,
	size_t schema_len, const char *table, size_t table_len, const char *column,
	size_t column_len, dbe_res_t **dbe_res
) {
	regexp *re_tb = NULL, *re_cl = NULL;
	int sft, tc = 0, i, j, r, radix, ord, dec, bl, ocl, nullable;
	char **tn = NULL, tmp[1024], *s1, *tablename = NULL;
	DIR *dh;
	Direntry_t *de;
	size_t len;
	csv_table_def_t *table_def;
	struct st_csv_parse parse;
	dbe_res_t *pres = NULL;
	csv_column_def_t *col;
	dbe_str_t data[18];
	
	Zero( data, 18, dbe_str_t );
	Zero( &parse, 1, struct st_csv_parse );
	parse.csv = &con->csv;
	if( table_len ) {
		re_tb = g_dbe->regexp_from_pattern( table, table_len, FALSE );
		if( re_tb == NULL ) {
			sft = 0;
			Newx( tablename, table_len + 1, char );
			Copy( table, tablename, table_len + 1, char );
			table_len = g_dbe->unescape_pattern( tablename, table_len, '\\' );
			table = tablename;
		}
		else {
			sft = 1;
		}
	}
	else
		sft = 1;
	if( sft ) {
		if( con->csv.path != NULL )
			s1 = my_strcpy( tmp, con->csv.path );
		else
			s1 = my_strcpy( tmp, "." );
		if( (dh = PerlDir_open( tmp )) == NULL ) {
			g_dbe->set_error( con->pcon,
				-1, "Unable to open directory '%s'", tmp );
			return DBE_ERROR;
		}
		while( (de = PerlDir_read( dh )) != NULL ) {
			if( de->d_name[0] == '.' )
				continue;
			len = strlen( de->d_name );
			if( ! g_dbe->regexp_match( re_tb, de->d_name, len ) )
				continue;
#ifdef CSV_DEBUG
			_debug( "entry %s\n", de->d_name );
#endif
			if( (tc % 8) == 0 )
				Renew( tn, tc + 8, char * );
			Newx( tn[tc], len + 1, char );
			strcpy( tn[tc], de->d_name );
			tc ++;
		}
	}
	else {
		Newx( tn, 1, char * );
		tn[0] = (char *) table;
		tc = 1;
	}
	if( column_len )
		re_cl = g_dbe->regexp_from_pattern( column, column_len, TRUE );
	pres = g_dbe->vres_create( con->pcon );
	g_dbe->vres_add_column( pres, "TABLE_CAT", 9, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "TABLE_SCHEM", 11, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "TABLE_NAME", 10, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "COLUMN_NAME", 11, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "DATA_TYPE", 9, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "TYPE_NAME", 9, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "COLUMN_SIZE", 11, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "BUFFER_LENGTH", 13, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "DECIMAL_DIGITS", 14, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "NUM_PREC_RADIX", 14, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "NULLABLE", 8, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "REMARKS", 7, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "COLUMN_DEF", 10, DBE_TYPE_VARCHAR );
	g_dbe->vres_add_column( pres, "SQL_DATA_TYPE", 13, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "SQL_DATETIME_SUB", 16, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "CHAR_OCTET_LENGTH", 17, DBE_TYPE_BIGINT );
	g_dbe->vres_add_column( pres, "ORDINAL_POSITION", 16, DBE_TYPE_INTEGER );
	g_dbe->vres_add_column( pres, "IS_NULLABLE", 11, DBE_TYPE_VARCHAR );
	data[7].value = (char *) &bl;
	data[10].value = (char *) &nullable, nullable = 1;
	data[16].value = (char *) &ord;
	SET_FIELD( data[17], "YES", 3 );
	for( i = 0; i < tc; i ++ ) {
		table_def = csv_open_table( &parse, tn[i], 0, NULL, 0, "r" );
		if( table_def == NULL )
			goto error;
		SET_FIELD( data[2], table_def->name, table_def->name_length );
		for( j = 0; j < (int) table_def->column_count; j ++ ) {
			col = &table_def->columns[j];
			if( re_cl != NULL ) {
				if( ! g_dbe->regexp_match( re_cl, col->name, col->name_length ) )
					continue;
			}
			SET_FIELD( data[3], col->name, col->name_length );
			data[4].value = (char *) &col->type;
			SET_FIELD( data[5], col->typename, col->typename_length );
			data[6].value = (char *) &col->size;
			switch( col->type ) {
			case FIELD_TYPE_INTEGER:
				bl = sizeof(int);
				radix = 10;
				data[8].value = NULL;
				data[9].value = (char *) &radix;
				data[15].value = NULL;
				break;
			case FIELD_TYPE_DOUBLE:
				bl = sizeof(double);
				radix = 2;
				data[8].value = (char *) &dec, dec = 15;
				data[9].value = (char *) &radix;
				data[15].value = NULL;
				break;
			case FIELD_TYPE_CHAR:
				bl = (int) col->size * 3 + 1;
				data[8].value = NULL;
				data[9].value = NULL;
				data[15].value = (char *) &ocl, ocl = (int) col->size * 3;
				break;
			default:
				bl = (int) col->size;
				data[8].value = NULL;
				data[9].value = NULL;
				data[15].value = (char *) &ocl, ocl = (int) col->size;
				break;
			}
			SET_FIELD( data[12], col->defval, col->defval_length );
			data[13].value = data[4].value;
			ord = j + 1;
			if( g_dbe->vres_add_row( pres, data ) != DBE_OK )
				goto error;
		}
		csv_table_def_free( table_def );
	}
	r = DBE_OK;
	goto exit;
error:
	r = DBE_ERROR;
exit:
	if( sft ) {
		for( tc --; tc >= 0; tc -- )
			Safefree( tn[tc] );
	}
	Safefree( tn );
	Safefree( tablename );
	if( re_tb != NULL )
		g_dbe->regexp_free( re_tb );
	if( re_cl != NULL )
		g_dbe->regexp_free( re_cl );
	(*dbe_res) = pres;
	return r;
}

/* global pointer to dbe structure */
const dbe_t *g_dbe = NULL;

/*
_g_drv_def ()
*/

/* driver definition structure */
const drv_def_t g_drv_def = {
	DBE_VERSION,
	DRV_NAME " " XS_VERSION " build " DRV_BUILD_STRING,
	dbe_mem_free,
	dbe_drv_free,
	dbe_drv_connect,
	dbe_con_free,
	dbe_con_ping,
	dbe_con_reconnect,
	NULL, /* con_set_trace */
	dbe_con_set_attr,
	dbe_con_get_attr,
	NULL, /* con_row_limit */
	dbe_con_query,
	NULL,
	dbe_con_affected_rows,
	NULL,
	NULL,
	NULL,
	NULL,
	dbe_con_prepare,
	dbe_stmt_free,
	dbe_stmt_param_count,
	dbe_stmt_bind_param,
	NULL, /* stmt_param_position */
	dbe_stmt_execute,
	dbe_res_free,
	dbe_res_num_rows,
	dbe_res_fetch_names,
	dbe_res_fetch_row,
	dbe_res_row_seek,
	dbe_res_row_tell,
	dbe_res_fetch_field,
	dbe_res_field_seek,
	dbe_res_field_tell,
	NULL, /* lob_size */
	NULL, /* lob_read */
	NULL, /* lob_write */
	NULL, /* lob_close */
	dbe_con_quote,
	dbe_con_quote_bin,
	dbe_con_quote_id,
	dbe_con_getinfo,
	dbe_con_type_info,
	NULL, /* con_data_sources */
	dbe_con_tables,
	dbe_con_columns
};
