#include "parse_int.h"

void csv_table_def_free( csv_table_def_t *table ) {
	size_t i;
	if( table == NULL )
		return;
	if( table->alias != table->name )
		Safefree( table->alias );
	Safefree( table->name );
	for( i = 0; i < table->column_count; i ++ )
		csv_column_def_free( &table->columns[i] );
	Safefree( table->columns );
	if( table->dstream != NULL )
		PerlIO_close( table->dstream );
	Safefree( table->data );
	Safefree( table );
}

int get_field_type_by_name( const char *name ) {
	if( my_stricmp( name, "INT" ) == 0
		|| my_stricmp( name, "INTEGER" ) == 0
		|| my_stricmp( name, "TINYINT" ) == 0
		|| my_stricmp( name, "LONGINT" ) == 0
		|| my_stricmp( name, "SMALLINT" ) == 0
	) {
		return FIELD_TYPE_INTEGER;
	}
	else if( my_stricmp( name, "FLOAT" ) == 0
		|| my_stricmp( name, "DOUBLE" ) == 0
		|| my_stricmp( name, "SINGLE" ) == 0
		|| my_stricmp( name, "DECIMAL" ) == 0
		|| my_stricmp( name, "REAL" ) == 0
	) {
		return FIELD_TYPE_DOUBLE;
	}
	else if(
		my_stricmp( name, "BLOB" ) == 0
		|| my_stricmp( name, "BINARY" ) == 0
		|| my_stricmp( name, "VARBINARY" ) == 0
	)
	{
		return FIELD_TYPE_BLOB;
	}
	return FIELD_TYPE_CHAR;
}

size_t get_column_size( int type, size_t size ) {
	switch( type ) {
	case FIELD_TYPE_INTEGER:
		return 8;
	case FIELD_TYPE_DOUBLE:
		return 17;
	case FIELD_TYPE_CHAR:
		return size > 0 ? size : 65535;
	case FIELD_TYPE_BLOB:
		return size > 0 ? size : 65535;
	}
	return 1;
}

char *csv_read_row( csv_table_def_t *table ) {
	csv_column_def_t *col;
	int ch, sp = 0, esc = 0;
	size_t pos = 0, cpos = 0;
	PerlIO *stream = table->dstream;
	col = &table->columns[cpos ++];
	col->offset = 0;
	while( 1 ) {
		ch = PerlIO_getc( stream );
_retry:
		switch( ch ) {
		case EOF:
			if( ! pos )
				return NULL;
			goto _finish;
		case '\n':
			if( ! sp )
				goto _finish;
			else
				goto _setchar;
		case '\r':
			if( ! sp )
				continue;
			else
				goto _setchar;
		case '\\':
			if( esc ) {
				esc = 0;
				goto _setchar;
			}
			esc = 1;
			ch = PerlIO_getc( stream );
			goto _retry;
		case ' ':
			if( ! esc && ! sp )
				continue;
			goto _setchar;
		case '\'':
			if( esc )
				goto _setchar;
			if( ! sp )
				sp = 1;
			else if( sp == 1 ) {
				ch = PerlIO_getc( stream );
				if( ch != '\'' ) {
					sp = 0;
					goto _retry;
				}
				goto _setchar;
			}
			else
				goto _setchar;
			continue;
		case '"':
			if( esc )
				goto _setchar;
			if( ! sp )
				sp = 2;
			else if( sp == 2 ) {
				ch = PerlIO_getc( stream );
				if( ch != '"' ) {
					sp = 0;
					goto _retry;
				}
				goto _setchar;
			}
			else
				goto _setchar;
			continue;
		case 'n':
			if( esc )
				ch = '\n';
			goto _setchar;
		case 'r':
			if( esc )
				ch = '\r';
			goto _setchar;
		case 't':
			if( esc )
				ch = '\t';
			goto _setchar;
		default:
			if( table->charset == CS_UTF8 && (ch & 0x80) )
				goto _setutf8;
			if( ! sp && ch == table->delimiter ) {
				if( cpos >= table->column_count ) {
					// fatal
					Perl_croak( aTHX_
						"Column count exceeds in table %s at row %u",
						table->name, table->row_pos
					);
				}
				col->length = pos - col->offset;
				col = &table->columns[cpos ++];
#ifdef CSV_DEBUG
				_debug( "column %s offset %u\n", col->name, pos + 1 );
#endif
				col->offset = pos + 1;
				ch = '\0';
				goto _setchar;
			}
		}
_setchar:
		if( pos >= table->data_size ) {
			table->data_size += 32;
			Renew( table->data, table->data_size + 1, char );
		}
		table->data[pos ++] = (char) ch;
		esc = 0;
		continue;
_setutf8:
		if( pos + 2 >= table->data_size ) {
			table->data_size += 32;
			Renew( table->data, table->data_size + 1, char );
		}
		table->data[pos ++] = (char) ch;
		ch &= 0x3F;
		if( ch & 0x20 ) {
			ch = PerlIO_getc( stream );
			if( ch == EOF )
				goto _finish;
			table->data[pos ++] = (char) ch;
		}
		ch = PerlIO_getc( stream );
		if( ch == EOF )
			goto _finish;
		table->data[pos ++] = (char) ch;
		esc = 0;
	}
_finish:
	col->length = pos - col->offset;
	table->row_pos ++;
	table->data[pos] = '\0';
	table->row_size = pos;
	return &table->data[pos];
}

csv_table_def_t *csv_open_table(
	csv_parse_t *parse, const char *tablename, size_t tablename_length,
	const char *alias, size_t alias_length, const char *mode
) {
	PerlIO *pfile;
	char tmp[1024], *s1;
	const char *cs1, *cs2;
	csv_t *csv = parse->csv;
	csv_column_def_t *column;
	csv_table_def_t *table = NULL;
	ini_file_t *ini = NULL;
	size_t i;
	if( parse->csv->flags & CSV_USESCHEMA ) {
		s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
		s1 = my_strcpy( s1, "schema.ini" );
		ini = inifile_open( tmp );
		if( ini == NULL )
			return NULL;
	}
	s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
	s1 = my_strcpy( s1, tablename );
retry:
	pfile = PerlIO_open( tmp, mode );
	if( pfile == NULL ) {
		char tmp2[1024];
    	s1 = parse->csv->path != NULL ? my_strcpy( tmp2, parse->csv->path ) : tmp2;
    	*s1 ++ = '~';
    	s1 = my_strcpy( s1, tablename );
    	pfile = PerlIO_open( tmp2, "r" );
    	if( pfile != NULL ) {
	    	PerlIO_close( pfile );
    		rename( tmp2, tmp );
    		goto retry;
    	}
		csv_error_msg(
			parse, CSV_ERR_UNKNOWNTABLE,
			"Table '%s' does not exist or is not readable", tablename
		);
		return NULL;
	}
#ifdef CSV_DEBUG
	_debug( "table '%s'\n", tablename );
#endif
	Newxz( table, 1, csv_table_def_t );
	table->dstream = pfile;
	table->delimiter = csv->delimiter;
	table->quote = csv->quote;
	table->max_scan_rows = csv->max_scan_rows;
	table->decimal_symbol = csv->decimal_symbol;
	table->charset = csv->table_charset;
	if( csv->flags & CSV_COLNAMEHEADER )
		table->flags = TEXT_TABLE_COLNAMEHEADER;
	if( ini != NULL ) {
		cs1 = inifile_read_string( ini, tablename, "Format", NULL );
		if( cs1 != NULL ) {
			if( (cs1 = strchr( cs1, '(' )) != NULL )
				table->delimiter = *(cs1 + 1);
			else if( my_stricmp( "CSVDELIMITED", cs1 ) == 0 )
				table->delimiter = ',';
			else if( my_stricmp( "TABDELIMITED", cs1 ) == 0 )
				table->delimiter = '\t';
		}
		cs1 = inifile_read_string( ini, tablename, "QuoteChar", NULL );
		if( cs1 != NULL && cs1[0] != '\0' )
			table->quote = cs1[0];
		cs1 = inifile_read_string( ini, tablename, "DecimalSymbol", NULL );
		if( cs1 != NULL && cs1[0] != '\0' )
			table->decimal_symbol = cs1[0];
		cs1 = inifile_read_string( ini, tablename, "ColNameHeader", NULL );
		if( cs1 != NULL ) {
			if( my_stricmp( "FALSE", cs1 ) == 0 ||
				my_stricmp( "NO", cs1 ) == 0 ||
				*cs1 == '0'
			)
				table->flags &= (~TEXT_TABLE_COLNAMEHEADER);
			else
				table->flags |= TEXT_TABLE_COLNAMEHEADER;
		}
		cs1 = inifile_read_string( ini, tablename, "CharacterSet", NULL );
		if( cs1 != NULL )
			table->charset = get_charset_id( cs1 );
		for( i = 1; ; i ++ ) {
			s1 = my_strcpy( tmp, "Col" );
			s1 = my_itoa( s1, (long) i, 10 );
			cs1 = inifile_read_string( ini, tablename, tmp, NULL );
			if( cs1 == NULL )
				break;
			if( (table->column_count % COLUMN_COUNT_EXPAND) == 0 )
				Renew(
					table->columns, table->column_count + COLUMN_COUNT_EXPAND,
					csv_column_def_t
				);
			column = &table->columns[table->column_count ++];
			Zero( column, 1, csv_column_def_t );
			// Name Type Width xx
			cs2 = strchr( cs1, ' ' );
			if( cs2 < cs1 )
				goto _col_check;
			column->name_length = cs2 - cs1;
			Newx( column->name, cs2 - cs1 + 1, char );
			Copy( cs1, column->name, cs2 - cs1, char );
			column->name[cs2 - cs1] = '\0';
			for( ; isSPACE( *cs2 ) && *cs2 != '\0'; cs2 ++ );
			cs1 = cs2;
			cs2 = strchr( cs1, ' ' );
			if( cs2 < cs1 )
				cs2 = strchr( cs1, '\0' );
			column->typename_length = cs2 - cs1;
			Newx( column->typename, cs2 - cs1 + 1, char );
			Copy( cs1, column->typename, cs2 - cs1, char );
			column->typename[cs2 - cs1] = '\0';
			for( ; isSPACE( *cs2 ) && *cs2 != '\0'; cs2 ++ );
			while( cs2 != NULL && *cs2 != '\0' ) {
				cs1 = cs2;
				cs2 = strchr( cs1, ' ' );
				if( cs2 < cs1 )
					goto _col_check;
				my_strncpyu( tmp, cs1, cs2 - cs1 );
				for( ; isSPACE( *cs2 ) && *cs2 != '\0'; cs2 ++ );
				if( strcmp( tmp, "WIDTH" ) == 0 )
					column->size = atoi( cs2 );
				else if( strcmp( tmp, "DEFAULT" ) == 0 ) {
					cs1 = cs2;
					if( *cs1 == '"' ) {
						cs1 ++;
						cs2 = strchr( cs1, '"' );
					}
					else if( *cs1 == '\'' ) {
						cs1 ++;
						cs2 = strchr( cs1, '\'' );
					}
					else {
						if( (cs2 = strchr( cs1, ' ' )) == NULL )
							cs2 = strchr( cs1, '\0' );
					}
					column->defval_length = cs2 - cs1;
					Newx( column->defval, cs2 - cs1 + 1, char );
					Copy( cs1, column->defval, cs2 - cs1, char );
					column->defval[cs2 - cs1] = '\0';
				}
			}
_col_check:
			column->type = column->typename != NULL
				? get_field_type_by_name( column->typename ) : FIELD_TYPE_CHAR;
			column->size = get_column_size( column->type, column->size );
#ifdef CSV_DEBUG
			_debug( "column %u name %s type %d %s width %u default %s\n",
				i, column->name, column->type, column->typename, column->size,
				column->defval );
#endif
			column->table = table;
			table->row_size += column->size;
		}
		inifile_close( ini );
	}
	if( table->columns == NULL ) {
		// read first line from csv
		int ch, sp = 0, esc = 0, *ctp;
		size_t j, k, pos = 0;
		while( 1 ) {
			ch = PerlIO_getc( pfile );
_retry:
			switch( ch ) {
			case EOF:
			case '\n':
				if( ! sp )
					goto _finish;
				else
					goto _setchar;
			case '\r':
				if( ! sp )
					continue;
				else
					goto _setchar;
			case '\\':
				if( esc ) {
					esc = 0;
					goto _setchar;
				}
				esc = 1;
				ch = PerlIO_getc( pfile );
				goto _retry;
			case ' ':
				if( ! esc && ! sp )
					continue;
				goto _setchar;
			case '\'':
				if( esc )
					goto _setchar;
				if( ! sp )
					sp = 1;
				else {
					ch = PerlIO_getc( pfile );
					if( ch != '\'' ) {
						sp = 0;
						goto _retry;
					}
					goto _setchar;
				}
				continue;
			case '"':
				if( esc )
					goto _setchar;
				if( ! sp )
					sp = 1;
				else {
					ch = PerlIO_getc( pfile );
					if( ch != '"' ) {
						sp = 0;
						goto _retry;
					}
					goto _setchar;
				}
				continue;
			default:
				if( ! sp && ch == table->delimiter ) {
					if( (table->column_count % COLUMN_COUNT_EXPAND) == 0 )
						Renew(
							table->columns,
							table->column_count + COLUMN_COUNT_EXPAND,
							csv_column_def_t
						);
					column = &table->columns[table->column_count ++];
					Zero( column, 1, csv_column_def_t );
					if( table->flags & TEXT_TABLE_COLNAMEHEADER ) {
						column->name_length = pos;
						Newx( column->name, pos + 1, char );
						Copy( table->data, column->name, pos, char );
						column->name[pos] = '\0';
					}
					else {
						Newx( column->name, 16, char );
						Copy( "Field", column->name, 5, char );
						column->name_length =
							my_itoa( &column->name[5], (long) table->column_count, 10 )
							- column->name;
					}
					/*
#ifdef CSV_DEBUG
					_debug( "found column [%s]\n", column->name );
#endif
					*/
					pos = 0;
					continue;
				}
			}
_setchar:
			if( pos >= table->data_size ) {
				table->data_size += 32;
				Renew( table->data, table->data_size + 1, char );
			}
			table->data[pos ++] = (char) ch;
			esc = 0;
		}
_finish:
		if( pos > 0 ) {
			if( (table->column_count % COLUMN_COUNT_EXPAND) == 0 )
				Renew(
					table->columns,
					table->column_count + COLUMN_COUNT_EXPAND,
					csv_column_def_t
				);
			column = &table->columns[table->column_count ++];
			Zero( column, 1, csv_column_def_t );
			if( table->flags & TEXT_TABLE_COLNAMEHEADER ) {
				column->name_length = pos;
				Newx( column->name, pos + 1, char );
				Copy( table->data, column->name, pos, char );
				column->name[pos] = '\0';
			}
			else {
				Newx( column->name, 16, char );
				Copy( "Field", column->name, 5, char );
				column->name_length =
					my_itoa( &column->name[5], (long) table->column_count, 10 )
					- column->name;
			}
			/*
#ifdef CSV_DEBUG
			_debug( "found column [%s]\n", column->name );
#endif
			*/
		}
		if( (table->flags & TEXT_TABLE_COLNAMEHEADER) == 0 )
			PerlIO_seek( pfile, 0, SEEK_SET );
		Newxz( ctp, table->column_count, int );
		for( i = 0; i < table->max_scan_rows; i ++ ) {
			if( (s1 = csv_read_row( table )) == NULL )
				break;
			for( j = 0; j < table->column_count; j ++ ) {
				column = &table->columns[j];
				if( ! column->length || column->type )
					continue;
				s1 = &table->data[column->offset];
				if( column->length == 4 && s1[0] == 'N' && s1[1] == 'U'
					&& s1[2] == 'L' && s1[3] == 'L'
				)
					continue;
				if( column->length >= 2 && s1[0] == '0' && s1[1] == 'x' ) {
					// binary
					column->type = FIELD_TYPE_BLOB;
				}
				else if( isDIGIT( s1[0] ) || s1[0] == '-' || s1[0] == '+' ) {
					if( ctp[j] )
						continue;
					ctp[j] = FIELD_TYPE_INTEGER;
					for( k = 1; k < column->length; k ++ ) {
						if( s1[k] == table->decimal_symbol ) {
							ctp[j] = FIELD_TYPE_DOUBLE;
						}
						else if( ! isDIGIT( s1[k] ) ) {
							ctp[j] = FIELD_TYPE_CHAR;
							break;
						}
					}
				}
				else {
					ctp[j] = FIELD_TYPE_CHAR;
				}
			}
		}
		for( j = 0; j < table->column_count; j ++ ) {
			column = &table->columns[j];
			if( ! column->type )
				column->type = ctp[j] ? ctp[j] : FIELD_TYPE_CHAR;
			switch( column->type ) {
			case FIELD_TYPE_INTEGER:
				Newx( column->typename, 8, char );
				Copy( "INTEGER", column->typename, 8, char );
				column->typename_length = 7;
				break;
			case FIELD_TYPE_DOUBLE:
				Newx( column->typename, 7, char );
				Copy( "DOUBLE", column->typename, 7, char );
				column->typename_length = 6;
				break;
			case FIELD_TYPE_BLOB:
				Newx( column->typename, 5, char );
				Copy( "BLOB", column->typename, 5, char );
				column->typename_length = 4;
				break;
			default:
				Newx( column->typename, 5, char );
				Copy( "CHAR", column->typename, 5, char );
				column->typename_length = 4;
				break;
			}
			column->size = get_column_size( column->type, 255 );
			column->table = table;
			table->row_size += column->size;
#ifdef CSV_DEBUG
			_debug( "column %u name %s type %d %s width %u default %s\n",
				j, column->name, column->type, column->typename, column->size,
				column->defval );
#endif
		}
		Safefree( ctp );
	}
	/* fit column size */
	if( (table->column_count % COLUMN_COUNT_EXPAND) != 0 )
		Renew( table->columns, table->column_count, csv_column_def_t );
	table->name_length = tablename_length
		? tablename_length : strlen( tablename );
	Newx( table->name, table->name_length + 1, char );
	Copy( tablename, table->name, table->name_length + 1, char );
	if( alias != NULL ) {
		if( ! alias_length )
			alias_length = strlen( alias );
		table->alias_length = alias_length;
		Newx( table->alias, alias_length + 1, char );
		Copy( alias, table->alias, alias_length, char );
		table->alias[table->alias_length] = '\0';
	}
	else if( (cs1 = strchr( tablename, '.' )) != NULL ) {
		table->alias_length = cs1 - tablename;
		Newx( table->alias, table->alias_length + 1, char );
		Copy( tablename, table->alias, table->alias_length, char );
		table->alias[table->alias_length] = '\0';
	}
	else {
		table->alias_length = table->name_length;
		table->alias = table->name;
	}
	return table;
}

void csv_start_table( csv_parse_t *parse, Expr *name, int exists ) {
	csv_table_def_t *table;
	const char *cs1;
	Newxz( table, 1, csv_table_def_t );
	table->name_length = name->var.sv_len;
	Newx( table->name, table->name_length + 1, char );
	Copy( name->var.sv, table->name, table->name_length + 1, char );
	if( (cs1 = strchr( table->name, '.' )) != NULL ) {
		table->alias_length = cs1 - table->name;
		Newx( table->alias, table->alias_length + 1, char );
		Copy( table->name, table->alias, table->alias_length, char );
		table->alias[table->alias_length] = '\0';
	}
	else {
		table->alias_length = table->name_length;
		table->alias = table->name;
	}
	table->delimiter = parse->csv->delimiter;
	table->quote = parse->csv->quote;
	table->max_scan_rows = parse->csv->max_scan_rows;
	table->decimal_symbol = parse->csv->decimal_symbol;
	if( parse->csv->flags & CSV_COLNAMEHEADER )
		table->flags = TEXT_TABLE_COLNAMEHEADER;
	parse->type = PARSE_TYPE_CREATE_TABLE;
	table->charset = parse->csv->table_charset;
	if( exists )
		parse->flags |= PARSE_CHECK_EXISTS;
	parse->table = table;
#ifdef CSV_DEBUG
	_debug( "CREATE TABLE [%s]\n", table->name );
#endif
	csv_expr_free( name );
}

void csv_table_add_column(
	csv_parse_t *parse, Expr *name, const char *typename,
	size_t typename_length, size_t size, ExprList *args
) {
	csv_column_def_t *col;
	csv_table_def_t *table = parse->table;
	size_t i;
	Expr *x;
	if( (table->column_count % COLUMN_COUNT_EXPAND) == 0 )
		Renew(
			table->columns, table->column_count + COLUMN_COUNT_EXPAND,
			csv_column_def_t
		);
	col = &table->columns[table->column_count ++];
	Zero( col, 1, csv_column_def_t );
	Newx( col->name, name->var.sv_len + 1, char );
	Copy( name->var.sv, col->name, name->var.sv_len + 1, char );
	col->name_length = name->var.sv_len;
	Newx( col->typename, typename_length + 1, char );
	my_strncpyu( col->typename, typename, typename_length );
	col->typename_length = typename_length;
	col->type = get_field_type_by_name( col->typename );
	col->size = size;
	if( args != NULL ) {
		for( i = 0; i < args->expr_count; i ++ ) {
			x = args->expr[i];
			switch( x->var.iv ) {
			case TK_DEFAULT:
				if( x->op ) {
					expr_eval( parse, x );
					VAR_EVAL_SV( x->var );
				}
#ifdef CSV_DEBUG
				_debug( "DEFAULT VALUE [%s]\n", x->var.sv );
#endif
				col->defval_length = x->var.sv_len;
				Renew( col->defval, col->defval_length + 1, char );
				Copy( x->var.sv, col->defval, col->defval_length + 1, char );
				break;
			}
		}
		csv_exprlist_free( args );
	}
#ifdef CSV_DEBUG
	_debug( "add_column [%s] type [%u][%s] size [%u]\n",
		name->var.sv, col->type, col->typename, size );
#endif
	csv_expr_free( name );
}

int csv_create_table( csv_parse_t *parse ) {
	PerlIO *pfile;
	ini_file_t *ini;
	char tmp[1024], *s1, *s2;
	csv_column_def_t *c1;
	csv_table_def_t *table = parse->table;
	size_t i;
	s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
	s1 = my_strcpy( s1, table->name );
	pfile = PerlIO_open( tmp, "r" );
	if( pfile != NULL ) {
		PerlIO_close( pfile );
		if( parse->flags & PARSE_CHECK_EXISTS )
			return CSV_OK;
		else {
			csv_error_msg( parse, CSV_ERR_CREATETABLE,
				"Table '%s' already exists", table->name );
			return CSV_ERROR;
		}
	}
	pfile = PerlIO_open( tmp, "wt" );
	if( pfile == NULL ) {
		csv_error_msg( parse, CSV_ERR_CREATETABLE,
			"Unable to create table '%s', check permissions", table->name );
		return CSV_ERROR;
	}
	// write column names
	if( table->flags & TEXT_TABLE_COLNAMEHEADER ) {
		for( i = 0; i < table->column_count; i ++ ) {
			c1 = &table->columns[i];
			if( i > 0 )
				PerlIO_putc( pfile, table->delimiter );
			WRITE_QUOTED( pfile, c1->name, c1->name_length, table->quote );
		}
		PerlIO_putc( pfile, '\n' );
	}
	table->dstream = pfile;
	if( (parse->csv->flags & CSV_USESCHEMA) == 0 )
		return CSV_OK;
	// fill schema.ini
	s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
	s1 = my_strcpy( s1, "schema.ini" );
	ini = inifile_open( tmp );
	if( ini == NULL )
		return CSV_OK;
	inifile_delete_section( ini, table->name );
	inifile_write_string( ini, table->name, "ColNameHeader", "True" );
	sprintf( tmp, "Delimited(%c)", table->delimiter );
	inifile_write_string( ini, table->name, "Format", tmp );
	inifile_write_char( ini,
		table->name, "DecimalSymbol", table->decimal_symbol );
	inifile_write_integer( ini,
		table->name, "MaxScanRows", (int) table->max_scan_rows );
	inifile_write_string( ini,
		table->name, "CharacterSet", get_charset_name( table->charset ) );
	sprintf( tmp, "%c", table->quote );
	inifile_write_string( ini, table->name, "QuoteChar", tmp );
	for( i = 0; i < table->column_count; i ++ ) {
		c1 = &table->columns[i];
		s1 = my_strcpy( tmp, "Col" );
		s1 = my_itoa( s1, (long) i + 1, 10 );
		s2 = s1 + 1;
		s1 = my_strcpy( s2, c1->name );
		*s1 ++ = ' ';
		s1 = my_strcpy( s1, c1->typename );
		if( c1->size > 0 ) {
			s1 = my_strcpy( s1, " Width " );
			s1 = my_itoa( s1, (long) c1->size, 10 );
		}
		if( c1->defval != NULL ) {
			s1 = my_strcpy( s1, " Default " );
			s1 = my_strcpy( s1, c1->defval );
		}
		inifile_write_string( ini, table->name, tmp, s2 );
	}
	inifile_close( ini );
	return CSV_OK;
}

void csv_start_drop_table( csv_parse_t *parse, Expr *name, int exists ) {
	csv_table_def_t *table;
	Newxz( table, 1, csv_table_def_t );
	Newx( table->name, name->var.sv_len + 1, char );
	Copy( name->var.sv, table->name, name->var.sv_len + 1, char );
	table->name_length = name->var.sv_len;
	parse->type = PARSE_TYPE_DROP_TABLE;
	if( exists )
		parse->flags |= PARSE_CHECK_EXISTS;
	parse->table = table;
	csv_expr_free( name );
}

int csv_drop_table( csv_parse_t *parse ) {
	PerlIO *pfile;
	ini_file_t *ini;
	char tmp[1024], *s1;
	csv_table_def_t *table = parse->table;
#ifdef CSV_DEBUG
	_debug( "DROP TABLE [%s]\n", table->name );
#endif
	s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
	s1 = my_strcpy( s1, table->name );
	pfile = PerlIO_open( tmp, "r" );
	if( pfile == NULL ) {
		if( parse->flags & PARSE_CHECK_EXISTS )
			return CSV_OK;
		else {
			s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
			*s1 ++ = '~';
			s1 = my_strcpy( s1, table->name );
			pfile = PerlIO_open( tmp, "r" );
			if( pfile != NULL )
				goto remove;
			csv_error_msg( parse, CSV_ERR_DROPTABLE,
				"Table '%s' does not exist", table->name );
			return CSV_ERROR;
		}
	}
remove:
	PerlIO_close( pfile );
	if( remove( tmp ) != 0 ) {
		csv_error_msg( parse, CSV_ERR_DROPTABLE,
			"Unable to drop table '%s', check permissions", table->name );
		return CSV_ERROR;
	}
	s1 = parse->csv->path != NULL ? my_strcpy( tmp, parse->csv->path ) : tmp;
	s1 = my_strcpy( s1, "schema.ini" );
	ini = inifile_open( tmp );
	if( ini == NULL )
		return CSV_OK;
	inifile_delete_section( ini, table->name );
	inifile_close( ini );
	return CSV_OK;
}
