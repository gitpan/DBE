#include "parse_int.h"

void csv_start_delete( csv_parse_t *parse, Expr *name, Expr *where ) {
	csv_delete_t *delete = NULL;
	csv_table_def_t *table = NULL;
	table = csv_open_table( parse,
		name->var.sv, name->var.sv_len, NULL, 0, "r+" );
	if( table == NULL )
		goto error;
	Newxz( delete, 1, csv_delete_t );
	if( where != NULL )
		if( CSV_OK != expr_bind_columns(
			parse, where, &table, 1, "where clause" ) )
				goto error;
	delete->table = table;
	delete->where = where;
	parse->type = PARSE_TYPE_DELETE;
	parse->delete = delete;
	csv_expr_free( name );
	return;
error:
	csv_table_def_free( table );
	csv_expr_free( where );
	Safefree( delete );
}

int csv_delete_run( csv_parse_t *parse ) {
	csv_delete_t *delete = parse->delete;
	csv_table_def_t *table = delete->table;
	csv_column_def_t *column;
	int rv;
	size_t i, rc = 0;
	Expr *x1 = delete->where;
	char tmppath[1024], tablepath[1024], *val;
	PerlIO *tfile;
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
	if( delete->where == NULL )
		goto finish;
	table->row_pos = 1;
	while( 1 ) {
		if( csv_read_row( table ) == NULL )
			goto finish;
		expr_eval( parse, x1 );
		if( x1->var.flags & VAR_HAS_IV ) {
			if( ! x1->var.iv )
				goto write_row;
		}
		else if( x1->var.flags & VAR_HAS_NV ) {
			if( ! x1->var.nv )
				goto write_row;
		}
		else if( x1->var.sv == NULL || x1->var.sv[0] == '0' )
			goto write_row;
		rc ++;
		continue;
write_row:
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
	PerlIO_close( tfile );
	parse->csv->affected_rows = rc;
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
	return CSV_OK;
}

void csv_delete_free( csv_delete_t *delete ) {
	if( delete == NULL )
		return;
	csv_expr_free( delete->where );
	csv_table_def_free( delete->table );
	Safefree( delete );
}
