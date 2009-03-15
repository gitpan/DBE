#include "parse_int.h"

int compare_sort_row( const void *a, const void *b );

/*
void print_result( csv_result_set_t *result ) {
	size_t i, j;
	csv_row_t *row;
	printf( "result %u columns %u rows\n", result->column_count, result->row_count );
	for( i = 0; i < result->column_count; i ++ ) {
		printf( "%s%s", i ? "|" : "", result->columns[i].name );
	}
	printf( "\n" );
	for( j = 0; j < result->row_count; j ++ ) {
		row = result->rows[j];
		for( i = 0; i < result->column_count; i ++ ) {
			if( row->data[i].flags & VAR_HAS_IV )
				printf( "%s%d", i ? "|" : "", row->data[i].iv );
			else if( row->data[i].flags & VAR_HAS_NV )
				printf( "%s%f", i ? "|" : "", row->data[i].nv );
			else
				printf( "%s%s", i ? "|" : "", row->data[i].sv );
		}
		printf( "\n" );
	}
}
*/

void csv_select_start( csv_parse_t *parse, csv_select_t *select ) {
	parse->select = select;
	parse->type = PARSE_TYPE_SELECT;
}

int csv_select_run_child(
	csv_parse_t *parse, csv_result_set_t *result, size_t table_pos
) {
	csv_select_t *select = parse->select;
	csv_table_def_t *table, *table2;
	char *s1, *s2;
	int ch, has_child = select->table_count - table_pos > 1;
	size_t i, j, k, l, m, rp = 0, nr = 0, l1;
	Expr *x1;
	ExprList *xl_cols = select->columns;
	csv_row_t *row;
	csv_var_t *v1, v2;
	csv_column_def_t *col;
	csv_t *csv = parse->csv;
	/* reset table */
	table = select->tables[table_pos];
	PerlIO_seek( table->dstream, table->header_offset, SEEK_SET );
	if( table->flags & TEXT_TABLE_COLNAMEHEADER ) {
		if( ! table->header_offset ) {
			do {
				ch = PerlIO_getc( table->dstream );
			} while( ch != EOF && ch != '\n' );
			table->header_offset = PerlIO_tell( table->dstream );
		}
	}
	table->row_pos = 0;
	/* read rows */
	while( (s1 = csv_read_row( table )) != NULL ) {
#ifdef CSV_DEBUG
		_debug( "table '%s' row %lu\n", table->name, table->row_pos );
#endif
		if( has_child ) {
			ch = csv_select_run_child( parse, result, table_pos + 1 );
			if( ch != CSV_OK )
				return ch;
			continue;
		}
		/* eval WHERE */
		if( (x1 = select->where) != NULL ) {
			ch = expr_eval( parse, x1 );
			if( ch != CSV_OK )
				return ch;
			if( x1->var.flags & VAR_HAS_IV ) {
				if( ! x1->var.iv )
					continue;
			}
			else if( x1->var.flags & VAR_HAS_NV ) {
				if( ! x1->var.nv )
					continue;
			}
			else if( x1->var.sv == NULL || x1->var.sv[0] == '0' )
				continue;
		}
		/* eval columns */
		for( i = 0; i < xl_cols->expr_count; i ++ ) {
			x1 = xl_cols->expr[i];
			if( (x1->flags & EXPR_IS_AGGR) == 0 ) {
				ch = expr_eval( parse, x1 );
				if( ch != CSV_OK )
					return ch;
			}
		}
		if( select->groupby == NULL )
			goto add_row;
		/* eval groupby */
		for( i = 0; i < select->groupby->expr_count; i ++ ) {
			ch = expr_eval( parse, select->groupby->expr[i] );
			if( ch != CSV_OK )
				return ch;
		}
		/* search groupby row */
		for( j = 0; j < result->row_count; j ++ ) {
			row = result->rows[j];
			for( i = 0; i < select->groupby->expr_count; i ++ ) {
				v1 = &row->groupby[i];
				x1 = select->groupby->expr[i];
				VAR_COMP_OP( v2, *v1, x1->var, ==, "==", 0, 0 );
				if( ! v2.iv )
					goto groupby_next;
			}
			goto eval_row;
groupby_next:
			continue;
		}
add_row:
		if( select->offset && rp < select->offset ) {
			rp ++;
			continue;
		}
		if( select->limit && rp - select->offset >= select->limit )
			goto finish;
		rp ++;
		Newxz( row, 1, csv_row_t );
		row->select = select;
		if( (result->row_count % ROW_COUNT_EXPAND) == 0 ) {
			Renew( result->rows, result->row_count + ROW_COUNT_EXPAND,
				csv_row_t * );
		}
		result->rows[result->row_count ++] = row;
		Newxz( row->data, result->column_count, csv_var_t );
		if( select->orderby != NULL )
			Newxz( row->orderby, select->orderby->expr_count, csv_var_t );
		if( select->groupby != NULL )
			Newxz( row->groupby, select->groupby->expr_count, csv_var_t );
		nr = 1;
eval_row:
		for( i = 0, j = 0; i < xl_cols->expr_count; i ++, j ++ ) {
			x1 = xl_cols->expr[i];
			if( x1->op == TK_TABLE ) {
				table2 = x1->table;
				for( l = 0; l < table2->column_count; l ++, j ++ ) {
					col = table2->columns + l;
					v1 = row->data + j;
					s1 = table->data + col->offset;
					l1 = col->length;
					if( l1 == 4 && strcmp( s1, "NULL" ) == 0 )
						continue;
					switch( col->type ) {
					case FIELD_TYPE_INTEGER:
						v1->iv = atoi( s1 );
						v1->flags = VAR_HAS_IV;
						break;
					case FIELD_TYPE_DOUBLE:
						if( table2->decimal_symbol != '.' ) {
							s2 = strchr( s1, table2->decimal_symbol );
							if( s2 != NULL )
								*s2 = '.';
						}
						v1->nv = atof( s1 );
						v1->flags = VAR_HAS_NV;
						break;
					case FIELD_TYPE_CHAR:
						v1->sv_len = charset_convert(
							s1, l1, table2->charset,
							csv->client_charset, &v1->sv
						);
						v1->flags = VAR_HAS_SV;
						break;
					case FIELD_TYPE_BLOB:
						if( l1 >= 2 && s1[0] == '0' && s1[1] == 'x' ) {
							l1 = (l1 - 2) / 2;
							if( ! l1 ) {
								v1->flags = 0;
								continue;
							}
							Renew( v1->sv, l1 + 1, char );
							for( m = 0, s1 += 2; m < l1; m ++ ) {
								ch = CHAR_FROM_HEX[*s1 & 0x7f] << 4;
								s1 ++;
								ch += CHAR_FROM_HEX[*s1 & 0x7f];
								s1 ++;
								v1->sv[m] = (char) ch;
							}
							v1->sv[l1] = '\0';
							v1->sv_len = l1;
							v1->flags = VAR_HAS_SV;
							break;
						}
					default:
						Renew( v1->sv, l1 + 1, char );
						Copy( s1, v1->sv, l1, char );
						v1->sv[l1] = '\0';
						v1->sv_len = l1;
						v1->flags = VAR_HAS_SV;
						break;
					}
				}
				continue;
			}
			if( x1->op == TK_ALL ) {
				for( k = 0; k < select->table_count; k ++ ) {
					table2 = select->tables[k];
					for( l = 0; l < table2->column_count; l ++, j ++ ) {
						col = table2->columns + l;
						v1 = row->data + j;
						s1 = table2->data + col->offset;
						l1 = col->length;
						if( l1 == 4 && strcmp( s1, "NULL" ) == 0 )
							continue;
						switch( col->type ) {
						case FIELD_TYPE_INTEGER:
							v1->iv = atoi( s1 );
							v1->flags = VAR_HAS_IV;
							break;
						case FIELD_TYPE_DOUBLE:
							if( table2->decimal_symbol != '.' ) {
								s2 = strchr( s1, table2->decimal_symbol );
								if( s2 != NULL )
									*s2 = '.';
							}
							v1->nv = atof( s1 );
							v1->flags = VAR_HAS_NV;
							break;
						case FIELD_TYPE_CHAR:
							v1->sv_len = charset_convert(
								s1, l1, table2->charset,
								csv->client_charset, &v1->sv
							);
							v1->flags = VAR_HAS_SV;
							break;
						case FIELD_TYPE_BLOB:
							if( l1 >= 2 && s1[0] == '0' && s1[1] == 'x' ) {
								l1 = (l1 - 2) / 2;
								if( ! l1 ) {
									v1->flags = 0;
									continue;
								}
								Renew( v1->sv, l1 + 1, char );
								for( m = 0, s1 += 2; m < l1; m ++ ) {
									ch = CHAR_FROM_HEX[*s1 & 0x7f] << 4;
									s1 ++;
									ch += CHAR_FROM_HEX[*s1 & 0x7f];
									s1 ++;
									v1->sv[m] = (char) ch;
								}
								v1->sv[l1] = '\0';
								v1->sv_len = l1;
								v1->flags = VAR_HAS_SV;
								break;
							}
						default:
							Renew( v1->sv, l1 + 1, char );
							Copy( s1, v1->sv, l1, char );
							v1->sv[l1] = '\0';
							v1->sv_len = l1;
							v1->flags = VAR_HAS_SV;
							break;
						}
					}
				}
				continue;
			}
			col = &result->columns[j];
			v1 = &row->data[j];
			if( x1->flags & EXPR_IS_AGGR ) {
				VAR_COPY( *v1, x1->var );
				x1->row = row;
				ch = expr_eval( parse, x1 );
				if( ch != CSV_OK )
					return ch;
			}
set_field:
			if( (x1->var.flags & (VAR_HAS_SV + VAR_HAS_NV + VAR_HAS_IV)) == 0 )
				continue;
			switch( col->type ) {
			case 0:
				if( x1->var.flags & VAR_HAS_IV ) {
					col->type = FIELD_TYPE_INTEGER;
					Newx( col->typename, 8, char );
					Copy( "INTEGER", col->typename, 8, char );
				}
				else if( x1->var.flags & VAR_HAS_NV ) {
					col->type = FIELD_TYPE_DOUBLE;
					Newx( col->typename, 7, char );
					Copy( "DOUBLE", col->typename, 7, char );
				}
				else if( x1->var.flags & VAR_HAS_SV ) {
					col->type = FIELD_TYPE_CHAR;
					Newx( col->typename, 5, char );
					Copy( "CHAR", col->typename, 5, char );
				}
				else
					continue;
				goto set_field;
			case FIELD_TYPE_INTEGER:
				VAR_EVAL_IV( x1->var );
				v1->iv = x1->var.iv;
				v1->flags = VAR_HAS_IV;
				break;
			case FIELD_TYPE_DOUBLE:
				VAR_EVAL_NV( x1->var );
				v1->nv = x1->var.nv;
				v1->flags = VAR_HAS_NV;
				break;
			case FIELD_TYPE_CHAR:
				VAR_EVAL_SV( x1->var );
				if( x1->var.sv_len ) {
					Renew( v1->sv, x1->var.sv_len + 1, char );
					Copy( x1->var.sv, v1->sv, x1->var.sv_len + 1, char );
					v1->sv_len = x1->var.sv_len;
					v1->flags = VAR_HAS_SV;
				}
				break;
			default:
				VAR_EVAL_SV( x1->var );
				if( x1->var.sv_len ) {
					Renew( v1->sv, x1->var.sv_len + 1, char );
					Copy( x1->var.sv, v1->sv, x1->var.sv_len + 1, char );
					v1->sv_len = x1->var.sv_len;
					v1->flags = VAR_HAS_SV;
				}
				break;
			}
		}
		if( nr ) {
			if( select->orderby != NULL ) {
				for( i = 0; i < select->orderby->expr_count; i ++ ) {
					x1 = select->orderby->expr[i];
					ch = expr_eval( parse, x1 );
					if( ch != CSV_OK )
						return ch;
					VAR_COPY( x1->var, row->orderby[i] );
				}
			}
			if( select->groupby != NULL ) {
				for( i = 0; i < select->groupby->expr_count; i ++ ) {
					x1 = select->groupby->expr[i];
					VAR_COPY( x1->var, row->groupby[i] );
				}
			}
			nr = 0;
		}
	}
finish:
	return CSV_OK;
}

csv_result_set_t *csv_select_run( csv_parse_t *parse ) {
	size_t i, j, k, l;
	ExprList *l1;
	Expr *x1;
	csv_t *csv = parse->csv;
	csv_table_def_t *table;
	csv_result_set_t *result;
	csv_column_def_t *column, *col2;
	csv_row_t *row;
	csv_select_t *select = parse->select;
	int r = CSV_OK;
	if( select == NULL )
		return NULL;
	Newxz( result, 1, csv_result_set_t );
	l1 = select->columns;
	if( select->tables == NULL ) {
		/* select without tables */
		result->column_count = l1->expr_count;
		Newxz( result->columns, result->column_count, csv_column_def_t );
		Newxz( row, 1, csv_row_t );
		Newxz( row->data, result->column_count, csv_var_t );
		for( i = 0; i < l1->expr_count; i ++ ) {
			x1 = l1->expr[i];
			if( x1->op ) {
				if( expr_eval( parse, x1 ) != CSV_OK ) {
					Safefree( row->data );
					Safefree( row );
					goto error;
				}
			}
			column = &result->columns[i];
			if( x1->alias_len ) {
				Newx( column->name, x1->alias_len + 1, char );
				Copy( x1->alias, column->name, x1->alias_len + 1, char );
				column->name_length = x1->alias_len;
			}
			else if( x1->token != NULL ) {
				column->name_length = x1->token->len;
				Newx( column->name, column->name_length + 1, char );
				Copy( x1->token->str, column->name, column->name_length, char );
				column->name[column->name_length] = '\0';
			}
			else {
				Newx( column->name, 16, char );
				Copy( "Field", column->name, 5, char );
				column->name_length =
					my_itoa( &column->name[5], (long) i + 1, 10 ) - column->name;
			}
			if( x1->var.flags & VAR_HAS_IV ) {
				column->type = FIELD_TYPE_INTEGER;
				Newx( column->typename, 8, char );
				Copy( "INTEGER", column->typename, 8, char );
				column->typename_length = 7;
				row->data[i].iv = x1->var.iv;
				row->data[i].flags = VAR_HAS_IV;
			}
			else if( x1->var.flags & VAR_HAS_NV ) {
				column->type = FIELD_TYPE_DOUBLE;
				Newx( column->typename, 7, char );
				Copy( "DOUBLE", column->typename, 7, char );
				column->typename_length = 6;
				row->data[i].nv = x1->var.nv;
				row->data[i].flags = VAR_HAS_NV;
			}
			else {
				column->type = FIELD_TYPE_CHAR;
				Newx( column->typename, 5, char );
				Copy( "CHAR", column->typename, 5, char );
				column->typename_length = 4;
				column->size = x1->var.sv_len;
				if( x1->var.sv != NULL ) {
					Newx( row->data[i].sv, x1->var.sv_len + 1, char );
					Copy( x1->var.sv, row->data[i].sv, x1->var.sv_len, char );
					row->data[i].sv[x1->var.sv_len] = '\0';
					row->data[i].sv_len = x1->var.sv_len;
					row->data[i].flags = VAR_HAS_SV;
				}
			}
		}
		Newx( result->rows, 1, csv_row_t * );
		result->rows[0] = row;
		result->row_count = 1;
		goto finish;
	}
	/* prepare columns */
	Newx( result->columns, l1->expr_count, csv_column_def_t );
	for( i = 0, j = 0; i < l1->expr_count; i ++ ) {
		x1 = l1->expr[i];
		if( x1->op == TK_TABLE ) {
			table = x1->table;
			Renew(
				result->columns, l1->expr_count - 1 + table->column_count,
				csv_column_def_t
			);
			for( l = 0; l < table->column_count; l ++, j ++ ) {
				column = &table->columns[l];
				col2 = &result->columns[j];
				Zero( col2, 1, csv_column_def_t );
				col2->name_length = column->name_length;
				Newx( col2->name, col2->name_length + 1, char );
				Copy( column->name,
					col2->name, col2->name_length + 1, char );
				col2->typename_length = column->typename_length;
				Newx( col2->typename, col2->typename_length + 1, char );
				Copy( column->typename,
					col2->typename, col2->typename_length + 1, char );
				col2->size = column->size;
				col2->type = column->type;
				col2->tablename_length = table->name_length;
				Newx( col2->tablename, table->name_length + 1, char );
				Copy( table->name,
					col2->tablename, table->name_length + 1, char );
			}
			continue;
		}
		if( x1->op == TK_ALL ) {
			for( k = 0; k < select->table_count; k ++ ) {
				table = select->tables[k];
				Renew(
					result->columns, l1->expr_count + j + table->column_count,
					csv_column_def_t
				);
				for( l = 0; l < table->column_count; l ++, j ++ ) {
					column = table->columns + l;
					col2 = result->columns + j;
					Zero( col2, 1, csv_column_def_t );
					col2->name_length = column->name_length;
					Newx( col2->name, col2->name_length + 1, char );
					Copy( column->name,
						col2->name, col2->name_length + 1, char );
					col2->typename_length = column->typename_length;
					Newx( col2->typename, col2->typename_length + 1, char );
					Copy( column->typename,
						col2->typename, col2->typename_length + 1, char );
					col2->size = column->size;
					col2->type = column->type;
					col2->tablename_length = table->name_length;
					Newx( col2->tablename, table->name_length + 1, char );
					Copy( table->name,
						col2->tablename, table->name_length + 1, char );
				}
			}
			continue;
		}
		column = result->columns + j ++;
		Zero( column, 1, csv_column_def_t );
		if( x1->alias_len ) {
			Newx( column->name, x1->alias_len + 1, char );
			Copy( x1->alias, column->name, x1->alias_len + 1, char );
			column->name_length = x1->alias_len;
		}
		else if( x1->flags & EXPR_IS_ID ) {
			Newx( column->name, x1->var.sv_len + 1, char );
			Copy( x1->var.sv, column->name, x1->var.sv_len + 1, char );
			column->name_length = x1->var.sv_len;
		}
		else if( x1->token != NULL ) {
			column->name_length = x1->token->len;
			Newx( column->name, column->name_length + 1, char );
			Copy( x1->token->str, column->name, column->name_length, char );
			column->name[column->name_length] = '\0';
		}
		else {
			Newx( column->name, 16, char );
			Copy( "Field", column->name, 5, char );
			column->name_length =
				my_itoa( &column->name[5], (long) j, 10 ) - column->name;
		}
		if( x1->flags & EXPR_IS_ID ) {
			column->typename_length = x1->column->typename_length;
			Newx( column->typename, column->typename_length + 1, char );
			Copy( x1->column->typename,
				column->typename, column->typename_length, char );
			column->size = x1->column->size;
			column->type = x1->column->type;
			table = x1->column->table;
			column->tablename_length = table->name_length;
			Newx( column->tablename, table->name_length + 1, char );
			Copy( table->name,
				column->tablename, table->name_length + 1, char );
		}
	}
#ifdef CSV_DEBUG
	_debug( "column count: %lu\n", j );
#endif
	result->column_count = j;
	r = csv_select_run_child( parse, result, 0 );
finish:
	/* order rows */
	if( select->orderby != NULL && result->row_count > 1 ) {
		qsort(
			result->rows, result->row_count, sizeof(csv_row_t *),
			compare_sort_row
		);
	}
	/* cleanup */
	if( result->row_count ) {
		if( select->orderby != NULL ) {
			if( select->groupby != NULL ) {
				k = select->orderby->expr_count;
				l = select->groupby->expr_count;
				for( i = result->row_count - 1; ; i -- ) {
					row = result->rows[i];
					for( j = k - 1; ; j -- ) {
						Safefree( row->orderby[j].sv );
						if( j == 0 )
							break;
					}
					Safefree( row->orderby );
					for( j = l - 1; ; j -- ) {
						Safefree( row->groupby[j].sv );
						if( j == 0 )
							break;
					}
					Safefree( row->groupby );
					if( i == 0 )
						break;
				}
			}
			else {
				k = select->orderby->expr_count;
				for( i = result->row_count - 1; ; i -- ) {
					row = result->rows[i];
					for( j = k - 1; ; j -- ) {
						Safefree( row->orderby[j].sv );
						if( j == 0 )
							break;
					}
					Safefree( row->orderby );
					if( i == 0 )
						break;
				}
			}
		}
		else if( select->groupby != NULL ) {
			l = select->groupby->expr_count;
			for( i = result->row_count - 1; ; i -- ) {
				row = result->rows[i];
				for( j = l - 1; ; j -- ) {
					Safefree( row->groupby[j].sv );
					if( j == 0 )
						break;
				}
				Safefree( row->groupby );
				if( i == 0 )
					break;
			}
		}
	}
	if( r != CSV_OK )
		goto error;
	csv->affected_rows = result->row_count;
	return result;
error:
	csv_result_free( result );
	return NULL;
}

static int src = 0;

int compare_sort_row( const void *a, const void *b ) {
	csv_row_t *r1 = *(csv_row_t **) a;
	csv_row_t *r2 = *(csv_row_t **) b;
	ExprList *orderby = r1->select->orderby;
	csv_var_t *v1, *v2;
	size_t i;
	int v = 0, asc;
	double d;
	if( r1 == r2 )
		return 0;
	src ++;
	for( i = 0; i < orderby->expr_count; i ++ ) {
		asc = orderby->expr[i]->flags & EXPR_SORT_ASC;
		v1 = &r1->orderby[i];
		v2 = &r2->orderby[i];
		if( ! v1->flags ) {
			if( v2->flags )
				return asc ? -1 : 1;
			continue;
		}
		else if( ! v2->flags ) {
			return asc ? 1 : -1;
		}
		if( v1->flags & VAR_HAS_IV ) {
			v = asc ? v1->iv - v2->iv : v2->iv - v1->iv;
#ifdef CSV_DEBUG
			_debug( "%d cmp field %u %d - %d = %d\n",
				src, i + 1, v1->iv, v2->iv, v );
#endif
		}
		else if( v1->flags & VAR_HAS_NV ) {
			d = v2->nv - v1->nv;
			v = d == 0 ? 0 : asc ? d < 0 ? 1 : -1 : d < 0 ? -1 : 1;
#ifdef CSV_DEBUG
			_debug( "%d cmp field %u %f - %f = %d\n",
				src, i + 1, v1->nv, v2->nv, v );
#endif
		}
		else if( v1->flags & VAR_HAS_SV ) {
			if( asc ) {
				if( v1->sv == NULL )
					v = v2->sv != NULL ? -1 : 0;
				else {
					if( v2->sv == NULL )
						v = 1;
					else
						STRICMP( v1->sv, v2->sv, v );
				}
			}
			else {
				if( v1->sv == NULL )
					v = v2->sv != NULL ? 1 : 0;
				else {
					if( v2->sv == NULL )
						v = -1;
					else
						STRICMP( v2->sv, v1->sv, v );
				}
			}
#ifdef CSV_DEBUG
			_debug( "%d cmp field %u %s - %s = %d\n",
				src, i + 1, v1->sv, v2->sv, v );
#endif
		}
		else {
#ifdef CSV_DEBUG
			_debug( "%d NO COMPARE\n", src );
#endif
		}
		if( v != 0 )
			return v;
	}
	return 0;
}

csv_select_t *csv_select_new(
	csv_parse_t *parse, ExprList *fields, ExprList *tables, ExprList *joins,
	Expr *where, ExprList *groupby, ExprList *orderby,
	size_t limit, size_t offset
) {
	csv_select_t *select;
	csv_table_def_t *table;
	size_t i, j;
	Expr *x1, *x2;
#ifdef CSV_DEBUG
	/*
	_debug( "csv_select_new\n" );
	printf( "COLUMNS\n" );
	print_exprlist( fields, 0 );
	printf( "TABLES\n" );
	print_exprlist( tables, 0 );
	printf( "WHERE\n" );
	print_expr( where, 0 );
	printf( "GROUP BY\n" );
	print_exprlist( groupby, 0 );
	printf( "ORDER BY\n" );
	print_exprlist( orderby, 0 );
	*/
	printf( "JOINS\n" );
	print_exprlist( joins, 0 );
#endif
	Newxz( select, 1, csv_select_t );
	if( tables != NULL ) {
		Newxz( select->tables, tables->expr_count, csv_table_def_t * );
		for( i = 0; i < tables->expr_count; i ++ ) {
			x1 = tables->expr[i];
			table = csv_open_table( parse,
				x1->var.sv, x1->var.sv_len, x1->alias, x1->alias_len, "r" );
			if( table == NULL )
				goto error;
			select->tables[select->table_count ++] = table;
		}
		csv_exprlist_free( tables );
		for( i = 0; i < fields->expr_count; i ++ )
			if( CSV_OK != expr_bind_columns(
				parse, fields->expr[i], select->tables, select->table_count,
				"field list"
			) )
				goto error;
		if( where != NULL )
			if( CSV_OK != expr_bind_columns(
				parse, where, select->tables, select->table_count,
				"where clause"
			) )
				goto error;
		if( groupby != NULL ) {
			for( i = 0; i < groupby->expr_count; i ++ ) {
				x1 = groupby->expr[i];
				if( x1->op == TK_DOT ) {
					/* expr is a table.field identifier */
					goto groupby_bind;
				}
				for( j = 0; j < fields->expr_count; j ++ ) {
					x2 = fields->expr[j];
					if( x2->flags & EXPR_IS_ID ) {
						if( strcmp( x1->var.sv, x2->var.sv ) == 0 ) {
							x1->ref = x2;
							x1->flags |= EXPR_USE_REF;
							goto groupby_next;
						}
					}
				}
groupby_bind:
				if( CSV_OK != expr_bind_columns(
					parse, groupby->expr[i], select->tables,
					select->table_count,
					"group clause"
				) )
					goto error;
groupby_next:
				continue;
			}
		}
		else if( parse->has_aggr ) {
			groupby = csv_exprlist_add( NULL,
				csv_expr( TK_ALL, NULL, NULL, NULL, NULL ), NULL );
		}
		if( orderby != NULL ) {
			for( i = 0; i < orderby->expr_count; i ++ ) {
				x1 = orderby->expr[i];
				if( x1->op == TK_DOT ) {
					/* expr is a table.field identifier */
					goto orderby_bind;
				}
				for( j = 0; j < fields->expr_count; j ++ ) {
					x2 = fields->expr[j];
					if( x2->flags & EXPR_IS_ID ) {
						if( strcmp( x1->var.sv, x2->var.sv ) == 0 ) {
							x1->ref = x2;
							x1->flags |= EXPR_USE_REF;
							goto orderby_next;
						}
					}
				}
orderby_bind:
				if( CSV_OK != expr_bind_columns(
					parse, x1, select->tables, select->table_count,
					"order clause"
				) )
					goto error;
orderby_next:
				continue;
			}
		}
	}
	select->columns = fields;
	select->where = where;
	select->groupby = groupby;
	select->orderby = orderby;
	select->limit = limit;
	select->offset = offset;
	return select;
error:
	csv_select_free( select );
	csv_exprlist_free( fields );
	csv_exprlist_free( groupby );
	csv_exprlist_free( orderby );
	csv_expr_free( where );
	return NULL;
}

void csv_select_free( csv_select_t *select ) {
	size_t i;
	if( select == NULL )
		return;
	csv_expr_free( select->where );
	csv_exprlist_free( select->columns );
	for( i = 0; i < select->table_count; i ++ )
		csv_table_def_free( select->tables[i] );
	csv_exprlist_free( select->groupby );
	csv_exprlist_free( select->orderby );
	Safefree( select->tables );
	Safefree( select );
}
