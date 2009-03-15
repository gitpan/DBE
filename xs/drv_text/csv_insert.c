#include "parse_int.h"

int csv_start_insert(
	csv_parse_t *parse, Expr *name, ExprList *cols, ExprList *values
) {
	csv_table_def_t *table;
	csv_insert_t *insert = NULL;
	size_t i, j;
	table = csv_open_table( parse,
		name->var.sv, name->var.sv_len, NULL, 0, "r+" );
	if( table == NULL )
		goto error;
	Newxz( insert, 1, csv_insert_t );
	Newxz( insert->values, table->column_count, Expr * );
	if( cols != NULL ) {
		if( values->expr_count != cols->expr_count ||
			values->expr_count > table->column_count
		) {
			csv_error_msg( parse, CSV_ERR_COLUMNCOUNT,
				"Number of columns differs to number of fields" );
			goto error;
		}
		for( i = 0; i < cols->expr_count; i ++ ) {
			if( CSV_OK != expr_bind_columns(
				parse, cols->expr[i], &table, 1, "insert clause" ) )
					goto error;
			if( CSV_OK != expr_bind_columns(
				parse, values->expr[i], &table, 1, "insert clause" ) )
					goto error;
			j = cols->expr[i]->column - table->columns;
			insert->values[j] = values->expr[i];
			values->expr[i] = NULL;
		}
	}
	else {
		if( values->expr_count != table->column_count ) {
			csv_error_msg( parse, CSV_ERR_COLUMNCOUNT,
				"Number of columns differs to number of fields" );
			goto error;
		}
		for( i = 0; i < values->expr_count; i ++ ) {
			if( CSV_OK != expr_bind_columns(
				parse, values->expr[i], &table, 1, "insert clause" ) )
					goto error;
			insert->values[i] = values->expr[i];
			values->expr[i] = NULL;
		}
	}
	insert->table = table;
	parse->type = PARSE_TYPE_INSERT;
	parse->insert = insert;
	csv_expr_free( name );
	csv_exprlist_free( cols );
	csv_exprlist_free( values );
	return CSV_OK;
error:
	csv_expr_free( name );
	csv_exprlist_free( cols );
	csv_exprlist_free( values );
	csv_table_def_free( table );
	Safefree( insert );
	return CSV_ERROR;
}

int csv_insert_run( csv_parse_t *parse ) {
	char *val, qf[1], qt[2], tmp[128], *p2;
	size_t i, j, len;
	PerlIO *pfile;
	csv_insert_t *insert = parse->insert;
	csv_table_def_t *table = insert->table;
	csv_column_def_t *col;
	Expr *x1;
	int ch;
	csv_var_t vt1;
	Zero( &vt1, 1, csv_var_t );
	pfile = table->dstream;
	PerlIO_seek( pfile, 0, SEEK_END );
	qf[0] = qt[1] = qt[2] = table->quote;
	for( i = 0; i < table->column_count; i ++ ) {
		if( i > 0 )
			PerlIO_putc( pfile, table->delimiter );
		col = &table->columns[i];
		x1 = insert->values[i];
		if( x1 != NULL && x1->op == TK_ISNULL ) {
			PerlIO_write( pfile, "NULL", 4 );
			continue;
		}
		switch( col->type ) {
		case FIELD_TYPE_CHAR:
			if( x1 != NULL ) {
				expr_eval( parse, x1 );
				VAR_EVAL_SV( x1->var );
				val = x1->var.sv;
				len = MIN( x1->var.sv_len, col->size );
			}
			else {
				val = col->defval;
				len = MIN( col->defval_length, col->size );
			}
#ifdef CSV_DEBUG
			_debug( "set %u char %u %s\n", i + 1, len, val );
#endif
			if( val != NULL ) {
				if( table->charset != parse->csv->client_charset ) {
					vt1.sv_len = charset_convert(
						val, len, parse->csv->client_charset,
						table->charset, &vt1.sv
					);
					val = vt1.sv;
					len = vt1.sv_len;
				}
				WRITE_QUOTED( pfile, val, len, table->quote );
			}
			else {
				PerlIO_write( pfile, "NULL", 4 );
			}
			break;
		case FIELD_TYPE_INTEGER:
			if( x1 != NULL ) {
				expr_eval( parse, x1 );
				VAR_EVAL_IV( x1->var );
				val = my_itoa( tmp, x1->var.iv, 10 );
			}
			else if( col->defval != NULL )
				val = my_itoa( tmp, atoi( col->defval ), 10 );
			else
				val = my_strcpy( tmp, "NULL" );
#ifdef CSV_DEBUG
			_debug( "set %u integer %s\n", i + 1, tmp );
#endif
			PerlIO_write( pfile, tmp, val - tmp );
			break;
		case FIELD_TYPE_DOUBLE:
			if( x1 != NULL ) {
				expr_eval( parse, x1 );
				VAR_EVAL_NV( x1->var );
				val = my_ftoa( tmp, x1->var.nv );
			}
			else if( col->defval != NULL ) {
				if( table->decimal_symbol != '.' )
					if( (p2 = strchr( col->defval, table->decimal_symbol )) != NULL )
						*p2 = '.';
				val = my_ftoa( tmp, atof( col->defval ) );
			}
			else
				val = my_strcpy( tmp, "NULL" );
#ifdef CSV_DEBUG
			_debug( "set %u integer %s\n", i + 1, tmp );
#endif
			if( table->decimal_symbol != '.' )
				if( (p2 = strchr( tmp, '.' )) != NULL )
					*p2 = table->decimal_symbol;
			PerlIO_write( pfile, tmp, val - tmp );
			break;
		case FIELD_TYPE_BLOB:
			if( x1 != NULL ) {
				expr_eval( parse, x1 );
				VAR_EVAL_SV( x1->var );
				val = x1->var.sv;
				len = MIN( x1->var.sv_len, col->size );
			}
			else {
				val = col->defval;
				len = MIN( col->defval_length, col->size );
			}
#ifdef CSV_DEBUG
			_debug( "set %u blob %s\n", i + 1, val );
#endif
			if( val != NULL ) {
				// convert to hex
				PerlIO_write( pfile, "0x", 2 );
				for( j = 0; j < len; j ++ ) {
					ch = (unsigned char) *val ++;
					PerlIO_putc( pfile, HEX_FROM_CHAR[ch / 16] );
					PerlIO_putc( pfile, HEX_FROM_CHAR[ch % 16] );
				}
			}
			else {
				PerlIO_write( pfile, "NULL", 4 );
			}
			break;
		}
	}
	PerlIO_putc( pfile, '\n' );
	parse->csv->affected_rows = 1;
	Safefree( vt1.sv );
	return CSV_OK;
}

void csv_insert_free( csv_insert_t *insert ) {
	size_t i;
	if( insert == NULL )
		return;
	for( i = 0; i < insert->table->column_count; i ++ ) {
		if( insert->values[i] != NULL )
			csv_expr_free( insert->values[i] );
	}
	csv_table_def_free( insert->table );
	Safefree( insert->values );
	Safefree( insert );
}
