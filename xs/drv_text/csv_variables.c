#include "parse_int.h"

void csv_set_start( csv_parse_t *parse, Expr *k, Expr *v ) {
	k->right = v;
	parse->expr = k;
	parse->type = PARSE_TYPE_SET;
}

int csv_set_run( csv_parse_t *parse ) {
	char *key = parse->expr->var.sv, *val = parse->expr->right->var.sv;
	my_strtolower( key );
#ifdef CSV_DEBUG
	_debug( "SET %s = %s\n", key, val );
#endif
	parse->csv->affected_rows = 0;
	switch( *key ) {
	case 'c':
		if( my_strncmp( key, "character_set_", 14 ) == 0 ) {
			enum n_charset cs = get_charset_id( val );
			if( cs == CS_UNKNOWN ) {
				csv_error_msg( parse, CSV_ERR_UNKNOWNCHARSET,
					"Unknown character set %s", val
				);
				return CSV_ERROR;
			}
			key += 14;
			if( strcmp( key, "table" ) == 0 ) {
				parse->csv->table_charset = cs;
				return CSV_OK;
			}
			else if( strcmp( key, "client" ) == 0 ) {
				parse->csv->client_charset = cs;
				return CSV_OK;
			}
			key -= 14;
		}
		else if( strcmp( key, "column_name_header" ) == 0 ) {
			if( *val == '0' ||
				my_stricmp( val, "FALSE" ) == 0 ||
				my_stricmp( val, "NO" ) == 0
				
			) {
				parse->csv->flags &= (~CSV_COLNAMEHEADER);
			}
			else {
				parse->csv->flags |= CSV_COLNAMEHEADER;
			}
			return CSV_OK;
		}
		break;
	case 'd':
		if( strcmp( key, "data_source" ) == 0 ) {
			return csv_set_path( parse->csv, val, 0 );
		}
		if( strcmp( key, "decimal_symbol" ) == 0 ) {
			parse->csv->decimal_symbol = *val;
			return CSV_OK;
		}
		if( strcmp( key, "delimiter" ) == 0 ) {
			parse->csv->delimiter = *val;
			return CSV_OK;
		}
		break;
	case 'm':
		if( strcmp( key, "max_scan_rows" ) == 0 ) {
			parse->csv->max_scan_rows = atoi( val );
			if( parse->csv->max_scan_rows <= 0 )
				parse->csv->max_scan_rows = 25;
			return CSV_OK;
		}
		break;
	case 'q':
		if( strcmp( key, "quote_char" ) == 0 ) {
			parse->csv->quote = *val;
			return CSV_OK;
		}
		break;
	case 'u':
		if( strcmp( key, "use_schema" ) == 0 ) {
			if( *val == '0' ||
				my_stricmp( val, "FALSE" ) == 0 ||
				my_stricmp( val, "NO" ) == 0
				
			) {
				parse->csv->flags &= (~CSV_USESCHEMA);
			}
			else {
				parse->csv->flags |= CSV_USESCHEMA;
			}
			return CSV_OK;
		}
		break;
	}
	csv_error_msg( parse, CSV_ERR_UNKNOWNVARIABLE,
		"Unknown variable '%s'", key
	);
	return CSV_ERROR;
}

void csv_start_show_variables( csv_parse_t *parse, Expr *like ) {
	parse->expr = like;
	parse->type = PARSE_TYPE_SHOW_VARS;
}

void csv_vcnv_charset( csv_var_t *s, csv_var_t *d ) {
	const char *tmp = get_charset_name( s->iv );
	VAR_SET_SV( *d, tmp, 0 );
}

enum n_variable_place {
	VAR_PLACE_CSV_T = 0,
	VAR_PLACE_LAST,
};

enum n_variable_type {
	VAR_TYPE_CHAR = 1,
	VAR_TYPE_UCHAR,
	VAR_TYPE_SHORT,
	VAR_TYPE_USHORT,
	VAR_TYPE_INT,
	VAR_TYPE_LONG,
	VAR_TYPE_FLOAT,
	VAR_TYPE_DOUBLE,
	VAR_TYPE_PCHAR,
};

struct st_variable {
	const char					*name;
	size_t						name_length;
	enum n_variable_type		type;
	enum n_variable_place		place;
	size_t						offset;
	void (*convert)( csv_var_t *s, csv_var_t *d );
	int							flag;
};

const struct st_variable variables[] = {
	{ "character_set_client", 20, VAR_TYPE_INT
		, VAR_PLACE_CSV_T, offsetof(csv_t, client_charset), csv_vcnv_charset },
	{ "character_set_table", 19, VAR_TYPE_INT
		, VAR_PLACE_CSV_T, offsetof(csv_t, table_charset), csv_vcnv_charset },
	{ "column_name_header", 18, VAR_TYPE_USHORT
		, VAR_PLACE_CSV_T, offsetof(csv_t, flags), NULL, CSV_COLNAMEHEADER },
	{ "data_source", 11, VAR_TYPE_PCHAR
		, VAR_PLACE_CSV_T, offsetof(csv_t, path) },
	{ "decimal_symbol", 14, VAR_TYPE_CHAR
		, VAR_PLACE_CSV_T, offsetof(csv_t, decimal_symbol) },
	{ "delimiter", 9, VAR_TYPE_CHAR
		, VAR_PLACE_CSV_T, offsetof(csv_t, delimiter) },
	{ "max_scan_rows", 13, VAR_TYPE_LONG
		, VAR_PLACE_CSV_T, offsetof(csv_t, max_scan_rows) },
	{ "quote_char", 10, VAR_TYPE_CHAR
		, VAR_PLACE_CSV_T, offsetof(csv_t, quote) },
	{ "use_schema", 10, VAR_TYPE_USHORT
		, VAR_PLACE_CSV_T, offsetof(csv_t, flags), NULL, CSV_USESCHEMA },
};
#define variable_count (sizeof(variables) / sizeof(struct st_variable))

csv_result_set_t *csv_run_show_variables( csv_parse_t *parse ) {
	csv_result_set_t *result;
	csv_column_def_t *column;
	csv_row_t *row;
	Expr *x = parse->expr;
	size_t i, j;
	const struct st_variable *var;
	csv_var_t *v, cv;
	regexp *re = NULL;
	const void *places[VAR_PLACE_LAST], *p;
	char tmp[32];
	places[VAR_PLACE_CSV_T] = parse->csv;
	if( x != NULL )
		re = g_dbe->regexp_from_pattern( x->var.sv, x->var.sv_len, TRUE );
	Newxz( result, 1, csv_result_set_t );
	/* set columns */
	result->column_count = 2;
	Newxz( result->columns, 2, csv_column_def_t );
	column = result->columns;
	Newx( column->name, 14, char );
	Copy( "Variable_name", column->name, 14, char );
	column->name_length = 13;
	column->type = FIELD_TYPE_CHAR;
	Newx( column->typename, 5, char );
	Copy( "CHAR", column->typename, 5, char );
	column->typename_length = 4;
	column->size = 255;
	column ++;
	Newx( column->name, 6, char );
	Copy( "Value", column->name, 6, char );
	column->name_length = 5;
	column->type = FIELD_TYPE_CHAR;
	Newx( column->typename, 5, char );
	Copy( "CHAR", column->typename, 5, char );
	column->typename_length = 4;
	column->size = 255;
	
	Zero( &cv, 1, csv_var_t );
	Newx( result->rows, variable_count, csv_row_t * );
	for( i = 0, j = 0; i < variable_count; i ++ ) {
		var = variables + i;
		if( re != NULL &&
			! g_dbe->regexp_match( re, var->name, var->name_length )
		) {
			continue;
		}
		Newxz( row, 1, csv_row_t );
		Newxz( row->data, 2, csv_var_t );
		/* set variable name */
		v = &row->data[0];
		Newx( v->sv, var->name_length + 1, char );
		Copy( var->name, v->sv, var->name_length + 1, char );
		v->sv_len = var->name_length;
		v->flags = VAR_HAS_SV;
		/* set variable value */
		v = &row->data[1];
		p = (const void *) ((size_t) places[var->place] + var->offset);
		switch( var->type ) {
		case VAR_TYPE_CHAR:
			tmp[0] = *((char *) p), tmp[1] = '\0';
			cv.sv = tmp;
			cv.sv_len = 1;
			VAR_FLAG_SV( cv );
			break;
		case VAR_TYPE_UCHAR:
			VAR_SET_IV( cv, *((unsigned char *) p) );
			break;
		case VAR_TYPE_SHORT:
			VAR_SET_IV( cv, *((short *) p) );
			break;
		case VAR_TYPE_USHORT:
			VAR_SET_IV( cv, *((unsigned short *) p) );
			break;
		case VAR_TYPE_INT:
			VAR_SET_IV( cv, *((int *) p) );
			break;
		case VAR_TYPE_LONG:
			VAR_SET_IV( cv, *((long *) p) );
			break;
		case VAR_TYPE_FLOAT:
			VAR_SET_NV( cv, *((float *) p) );
			break;
		case VAR_TYPE_DOUBLE:
			VAR_SET_NV( cv, *((double *) p) );
			break;
		case VAR_TYPE_PCHAR:
			cv.sv = *((char **) p);
			cv.sv_len = strlen( cv.sv );
			cv.flags = VAR_HAS_SV;
			break;
		}
		if( var->convert != NULL ) {
			var->convert( &cv, v );
		}
		else if( var->flag ) {
			VAR_EVAL_IV( cv );
			if( cv.iv & var->flag ) {
				VAR_SET_SV( *v, "YES", 3 );
			}
			else {
				VAR_SET_SV( *v, "NO", 2 );
			}
		}
		else {
			VAR_COPY( cv, *v );
		}
		VAR_EVAL_SV( *v );

		result->rows[j ++] = row;
	}
	result->row_count = j;
	if( j < variable_count )
		Renew( result->rows, j, csv_row_t * );
	if( re != NULL )
		g_dbe->regexp_free( re );
	return result;
}
