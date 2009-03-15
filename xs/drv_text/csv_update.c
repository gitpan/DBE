#include "parse_int.h"

void csv_start_update(
	csv_parse_t *parse, Expr *name, ExprList *setlist, Expr *where
) {
	csv_update_t *update = NULL;
	csv_table_def_t *table = NULL;
	csv_column_def_t *col;
	Expr **sl = NULL;
	size_t i, j, k;
	table = csv_open_table( parse,
		name->var.sv, name->var.sv_len, NULL, 0, "r+" );
	if( table == NULL )
		goto error;
	Newxz( update, 1, csv_update_t );
	for( i = 0; i < setlist->expr_count; i ++ )
		if( CSV_OK != expr_bind_columns(
			parse, setlist->expr[i], &table, 1, "set list" ) )
				goto error;
	if( where != NULL )
		if( CSV_OK != expr_bind_columns(
			parse, where, &table, 1, "where clause" ) )
				goto error;
	Newxz( sl, table->column_count, Expr * );
	k = setlist->expr_count;
	for( i = 0; i < table->column_count; i ++ ) {
		col = table->columns + i;
		for( j = 0; j < setlist->expr_count; j ++ ) {
			if( setlist->expr[j] == NULL )
				continue;
			if( setlist->expr[j]->column == col ) {
				sl[i] = setlist->expr[j];
				setlist->expr[j] = NULL;
				k --;
				break;
			}
		}
		if( ! k )
			break;
	}
	update->setlist = sl;
	update->table = table;
	update->where = where;
	parse->type = PARSE_TYPE_UPDATE;
	parse->update = update;
	csv_expr_free( name );
	csv_exprlist_free( setlist );
	return;
error:
	csv_table_def_free( table );
	csv_expr_free( where );
	csv_exprlist_free( setlist );
	Safefree( update );
	Safefree( sl );
}

int csv_update_run( csv_parse_t *parse ) {
	csv_update_t *update = parse->update;
	csv_table_def_t *table = update->table;
	csv_column_def_t *column;
	int rv, ch;
	size_t i, j, len, rc = 0;
	Expr *x1;
	Expr **sl = update->setlist;
	char tmppath[1024], tablepath[1024], *val, tmp[41], *p2;
	csv_var_t vt1;
	PerlIO *tfile;
	Zero( &vt1, 1, csv_var_t );
	val = parse->csv->path != NULL
		? my_strcpy( tmppath, parse->csv->path ) : tmppath;
	*val ++ = '~';
	val = my_strcpy( val, table->name );
	//val = my_strcpy( val, ".tmp" );
	tfile = PerlIO_open( tmppath, "wt" );
	if( tfile == NULL ) {
		csv_error_msg( parse, CSV_ERR_TMPFILE,
			"Unable to create tempory file (%s)", tmppath
		);
		return CSV_ERROR;
	}
	PerlIO_seek( table->dstream, 0, SEEK_SET );
	if( table->flags & TEXT_TABLE_COLNAMEHEADER ) {
		do {
			rv = PerlIO_getc( table->dstream );
			PerlIO_putc( tfile, rv );
		} while( rv != EOF && rv != '\n' );
	}
	table->row_pos = 1;
	while( 1 ) {
		if( csv_read_row( table ) == NULL )
			goto finish;
		if( (x1 = update->where) != NULL ) {
			expr_eval( parse, x1 );
			if( x1->var.flags & VAR_HAS_IV ) {
				if( ! x1->var.iv )
					goto write_raw;
			}
			else if( x1->var.flags & VAR_HAS_NV ) {
				if( ! x1->var.nv )
					goto write_raw;
			}
			else if( x1->var.sv == NULL || x1->var.sv[0] == '0' )
				goto write_raw;
		}
		for( i = 0; i < table->column_count; i ++ ) {
			if( i > 0 )
				PerlIO_putc( tfile, table->delimiter );
			column = table->columns + i;
			if( (x1 = sl[i]) != NULL ) {
				expr_eval( parse, x1 = x1->left );
				switch( column->type ) {
				case FIELD_TYPE_INTEGER:
					VAR_EVAL_IV( x1->var );
					val = my_itoa( tmp, x1->var.iv, 10 );
					PerlIO_write( tfile, tmp, val - tmp );
					break;
				case FIELD_TYPE_DOUBLE:
					VAR_EVAL_NV( x1->var );
					val = my_ftoa( tmp, x1->var.nv );
					if( table->decimal_symbol != '.' )
						if( (p2 = strchr( tmp, '.' )) != NULL )
							*p2 = table->decimal_symbol;
					PerlIO_write( tfile, tmp, val - tmp );
					break;
				case FIELD_TYPE_CHAR:
					VAR_EVAL_SV( x1->var );
					val = x1->var.sv;
					if( val != NULL ) {
						if( table->charset != parse->csv->client_charset ) {
							vt1.sv_len = charset_convert(
								val, x1->var.sv_len, parse->csv->client_charset,
								table->charset, &vt1.sv
							);
							val = vt1.sv;
							len = MIN( vt1.sv_len, column->size );
						}
						else {
							len = MIN( x1->var.sv_len, column->size );
						}
						WRITE_QUOTED( tfile, val, len, table->quote );
					}
					else
						PerlIO_write( tfile, "NULL", 4 );
					break;
				case FIELD_TYPE_BLOB:
					VAR_EVAL_SV( x1->var );
					val = x1->var.sv;
					len = MIN( x1->var.sv_len, column->size );
					if( val != NULL ) {
						// convert to hex
						PerlIO_write( tfile, "0x", 2 );
						for( j = 0; j < len; j ++ ) {
							ch = (unsigned char) *val ++;
							PerlIO_putc( tfile, HEX_FROM_CHAR[ch / 16] );
							PerlIO_putc( tfile, HEX_FROM_CHAR[ch % 16] );
						}
					}
					else
						PerlIO_write( tfile, "NULL", 4 );
					break;
				default:
					VAR_EVAL_SV( x1->var );
					val = x1->var.sv;
					len = MIN( x1->var.sv_len, column->size );
					if( val != NULL ) {
						WRITE_QUOTED( tfile, val, len, table->quote );
					}
					else
						PerlIO_write( tfile, "NULL", 4 );
					break;
				}
			}
			else {
				val = &table->data[column->offset];
				switch( column->type ) {
				case FIELD_TYPE_CHAR:
					WRITE_QUOTED( tfile, val, column->length, table->quote );
					break;
				default:
					PerlIO_write( tfile, val, column->length );
					break;
				}
			}
		}
		PerlIO_putc( tfile, '\n' );
		rc ++;
		continue;
write_raw:
		for( i = 0; i < table->column_count; i ++ ) {
			column = &table->columns[i];
			if( i > 0 )
				PerlIO_putc( tfile, table->delimiter );
			val = &table->data[column->offset];
			switch( column->type ) {
			case FIELD_TYPE_CHAR:
				WRITE_QUOTED( tfile, val, column->length, table->quote );
				break;
			default:
				PerlIO_write( tfile, val, column->length );
				break;
			}
		}
		PerlIO_putc( tfile, '\n' );
	}
finish:
	Safefree( vt1.sv );
	PerlIO_close( tfile );
	parse->csv->affected_rows = rc;
	if( rc ) {
		PerlIO_close( table->dstream );
		val = parse->csv->path != NULL
			? my_strcpy( tablepath, parse->csv->path ) : tablepath;
		val = my_strcpy( val, table->name );
		if( remove( tablepath ) != 0 ) {
			csv_error_msg( parse, CSV_ERR_REMOVETABLE,
				"Unable to remove table %s", table->name );
			table->dstream = PerlIO_open( tablepath, "r+t" );
			remove( tmppath );
			return CSV_ERROR;
		}
		rename( tmppath, tablepath );
		table->dstream = PerlIO_open( tablepath, "r+t" );
	}
	else {
		remove( tmppath );
	}
	return CSV_OK;
}

void csv_update_free( csv_update_t *update ) {
	size_t i;
	if( update == NULL )
		return;
	csv_expr_free( update->where );
	for( i = 0; i < update->table->column_count; i ++ )
		if( update->setlist[i] != NULL )
			csv_expr_free( update->setlist[i] );
	Safefree( update->setlist );
	csv_table_def_free( update->table );
	Safefree( update );
}
