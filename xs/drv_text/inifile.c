#include "inifile.h"

ini_file_t *inifile_open( const char *filename ) {
	ini_file_t *ini;
	Newxz( ini, 1, ini_file_t );
	my_strncpy( ini->filename, filename, PATH_MAX );
	inifile_load( ini );
	return ini;
}

void inifile_cleanup( ini_file_t *ini ) {
	ini_section_t *s1, *s2;
	ini_item_t *i1, *i2;
	if( ini == NULL )
		return;
	for( s1 = ini->first_section; s1 != NULL; ) {
		s2 = s1->next;
		for( i1 = s1->first_item; i1 != NULL; ) {
			i2 = i1->next;
			Safefree( i1->ident );
			Safefree( i1->value );
			Safefree( i1->p_comment );
			Safefree( i1 );
			i1 = i2;
		}
		Safefree( s1->s_comment );
		Safefree( s1->p_comment );
		Safefree( s1->name );
		Safefree( s1 );
		s1 = s2;
	}
	Safefree( ini->p_comment );
	Safefree( ini->newline );
	ini->first_section = ini->last_section = NULL;
	ini->p_comment = ini->newline = NULL;
}

void inifile_close( ini_file_t *ini ) {
	if( ini == NULL )
		return;
	if( ini->changed )
		inifile_save( ini );
	inifile_cleanup( ini );
	Safefree( ini );
}

int inifile_load( ini_file_t *ini ) {
	FILE *file;
	ini_section_t *sec = NULL;
	ini_item_t *item = NULL;
	char *stmp = NULL, *srmk = NULL;
	int c, step = 0, nl = 1, co = 0;
	size_t ltmp = 0, ptmp = 0, lrmk = 0, prmk = 0;
	inifile_cleanup( ini );
	file = fopen( ini->filename, "r" );
	if( file == NULL )
		return 1;
	while( (c = fgetc( file )) != EOF ) {
		switch( c ) {
		case '\r':
			if( ini->newline == NULL && nl == 1 ) {
				ini->newline_length = 2;
				Newx( ini->newline, 3, char );
				Copy( "\n\r", ini->newline, 3, char );
			}
			nl = 2;
			break;
		case '\n':
			if( ini->newline == NULL ) {
				if( nl == 2 ) {
					ini->newline_length = 2;
					Newx( ini->newline, 3, char );
					Copy( "\r\n", ini->newline, 3, char );
				}
				else {
					ini->newline_length = 1;
					Newx( ini->newline, 2, char );
					Copy( "\n", ini->newline, 2, char );
				}
			}
			if( co ) {
				co = 0;
				stmp[ptmp] = '\0';
				/*
#ifdef CSV_DEBUG
				_debug( "comment line read [%s]\n", stmp );
#endif
				*/
				if( prmk + ptmp > lrmk ) {
					lrmk = prmk + ptmp;
					Renew( srmk, lrmk + ini->newline_length, char );
				}
				Copy( stmp, &srmk[prmk], ptmp, char );
				prmk += ptmp;
				Copy( ini->newline, &srmk[prmk], ini->newline_length, char );
				prmk += ini->newline_length;
				ptmp = 0;
			}
			else if( step == 3 ) {
				for( nl = 0; nl < ptmp && isSPACE( stmp[nl] ); nl ++ );
				stmp[ptmp] = '\0';
				/*
#ifdef CSV_DEBUG
				_debug( "value read [%s]\n", &stmp[nl] );
#endif
				*/
				item->value_length = ptmp - nl;
				Newx( item->value, ptmp - nl + 1, char );
				Copy( &stmp[nl], item->value, ptmp - nl + 1, char );
				ptmp = 0;
				step = 2;
			}
			nl = 1;
			continue;
		case '[':
			if( nl ) {
				/*
#ifdef CSV_DEBUG
				_debug( "section begin\n" );
#endif
				*/
				Newxz( sec, 1, ini_section_t );
				item = NULL;
				if( prmk > 0 ) {
					srmk[prmk] = '\0';
					/*
#ifdef CSV_DEBUG
					_debug( "comment %u\n%s\n", prmk, srmk );
#endif
					*/
					sec->s_comment_length = prmk;
					Newx( sec->s_comment, prmk + 1, char );
					Copy( srmk, sec->s_comment, prmk + 1, char );
					prmk = 0;
				}
				step = 1;
				break;
			}
			goto _default;
		case ']':
			if( step == 1 ) {
				stmp[ptmp] = '\0';
				/*
#ifdef CSV_DEBUG
				_debug( "section read [%s]\n", stmp );
#endif
				*/
				sec->name_length = ptmp;
				Newx( sec->name, ptmp + 1, char );
				Copy( stmp, sec->name, ptmp + 1, char );
				if( ini->first_section == NULL )
					ini->first_section = sec;
				else {
					ini->last_section->next = sec;
					sec->prev = ini->last_section;
				}
				ini->last_section = sec;
				ptmp = 0;
				step = 2;
				break;
			}
			goto _default;
		case '=':
			if( ! co && step == 2 ) {
				if( prmk > 0 ) {
					srmk[prmk] = '\0';
					/*
#ifdef CSV_DEBUG
					_debug( "comment %u\n%s\n", prmk, srmk );
#endif
					*/
					if( item != NULL ) {
						item->p_comment_length = prmk;
						Newx( item->p_comment, prmk + 1, char );
						Copy( srmk, item->p_comment, prmk + 1, char );
					}
					else {
						sec->p_comment_length = prmk;
						Newx( sec->p_comment, prmk + 1, char );
						Copy( srmk, sec->p_comment, prmk + 1, char );
					}
					prmk = 0;
				}
				for( ; ptmp > 0 && isSPACE( stmp[ptmp - 1] ); ptmp -- );
				stmp[ptmp] = '\0';
				/*
#ifdef CSV_DEBUG
				_debug( "ident read [%s]\n", stmp );
#endif
				*/
				Newxz( item, 1, ini_item_t );
				item->ident_length = ptmp;
				Newx( item->ident, ptmp + 1, char );
				Copy( stmp, item->ident, ptmp + 1, char );
				if( sec->first_item == NULL )
					sec->first_item = item;
				else {
					sec->last_item->next = item;
					item->prev = sec->last_item;
				}
				sec->last_item = item;
				ptmp = 0;
				step = 3;
				break;
			}
			goto _default;
		case ';':
		case '#':
			if( nl )
				co = 1;
		default:
_default:
			if( ptmp >= ltmp ) {
				ltmp += 32;
				Renew( stmp, ltmp + 1, char );
			}
			stmp[ptmp ++] = (char) c;
			nl = 0;
			break;
		}
	}
	if( prmk > 0 ) {
		srmk[prmk] = '\0';
		/*
#ifdef CSV_DEBUG
		_debug( "comment %u\n%s\n", prmk, srmk );
#endif
		*/
		ini->p_comment_length = prmk;
		Newx( ini->p_comment, prmk + 1, char );
		Copy( srmk, ini->p_comment, prmk + 1, char );
	}
	fclose( file );
	Safefree( stmp );
	Safefree( srmk );
	return 0;
}

int inifile_save( ini_file_t *ini ) {
	ini_section_t *s1;
	ini_item_t *i1;
	char tmp[PATH_MAX], *p1;
	FILE *fh;
	p1 = my_strcpy( tmp, ini->filename );
	//p1 = my_strcpy( p1, ".txt" );
	fh = fopen( tmp, "w" );
	if( fh == NULL )
		return 1;
	//fseek( file, 0, SEEK_SET );
	if( ini->newline == NULL ) {
		ini->newline_length = sizeof( EOL );
		Newx( ini->newline, sizeof( EOL ) + 1, char );
		Copy( EOL, ini->newline, sizeof( EOL ) + 1, char );
	}
	for( s1 = ini->first_section; s1 != NULL; s1 = s1->next ) {
		if( s1->s_comment != NULL )
			fputs( s1->s_comment, fh );
		if( s1->prev != NULL )
			fputs( ini->newline, fh );
		fputc( '[', fh );
		fputs( s1->name, fh );
		fputc( ']', fh );
		fputs( ini->newline, fh );
		if( s1->p_comment != NULL )
			fputs( s1->p_comment, fh );
		for( i1 = s1->first_item; i1 != NULL; i1 = i1->next ) {
			fputs( i1->ident, fh );
			fputc( '=', fh );
			if( i1->value != NULL )
				fputs( i1->value, fh );
			fputs( ini->newline, fh );
			if( i1->p_comment != NULL )
				fputs( i1->p_comment, fh );
		}
	}
	if( ini->p_comment != NULL )
		fputs( ini->p_comment, fh );
	fclose( fh );
	ini->changed = 0;
	return 0;
}

const char *inifile_read_string(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	const char *s_default
) {
	ini_section_t *s1;
	ini_item_t *i1;
	for( s1 = ini->first_section; s1 != NULL; s1 = s1->next ) {
		if( my_stricmp( s1->name, s_section ) != 0 )
			continue;
		for( i1 = s1->first_item; i1 != NULL; i1 = i1->next ) {
			if( my_stricmp( i1->ident, s_ident ) != 0 )
				continue;
			return i1->value;
		}
	}
	return s_default;
}

void inifile_write_string(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	const char *s_value
) {
	ini_section_t *s1;
	ini_item_t *i1;
	size_t len = s_value != NULL ? strlen( s_value ) : 0;
	ini->changed = 1;
	for( s1 = ini->first_section; s1 != NULL; s1 = s1->next ) {
		if( my_stricmp( s1->name, s_section ) != 0 )
			continue;
		for( i1 = s1->first_item; i1 != NULL; i1 = i1->next ) {
			if( my_stricmp( i1->ident, s_ident ) != 0 )
				continue;
			if( len ) {
				if( i1->value_length < len )
					Renew( i1->value, len + 1, char );
				i1->value_length = len;
				Copy( s_value, i1->value, len + 1, char );
			}
			else
				Safefree( i1->value );
			return;
		}
		goto _create_item;
	}
	Newxz( s1, 1, ini_section_t );
	if( ini->first_section == NULL )
		ini->first_section = s1;
	else {
		ini->last_section->next = s1;
		s1->prev = ini->last_section;
	}
	ini->last_section = s1;
	s1->name_length = strlen( s_section );
	Newx( s1->name, s1->name_length + 1, char );
	Copy( s_section, s1->name, s1->name_length + 1, char );
_create_item:
	Newxz( i1, 1, ini_item_t );
	if( s1->first_item == NULL )
		s1->first_item = i1;
	else {
		s1->last_item->next = i1;
		i1->prev = s1->last_item;
	}
	s1->last_item = i1;
	i1->ident_length = strlen( s_ident );
	Newx( i1->ident, i1->ident_length + 1, char );
	Copy( s_ident, i1->ident, i1->ident_length + 1, char );
	if( len ) {
		i1->value_length = len;
		Newx( i1->value, len + 1, char );
		Copy( s_value, i1->value, len + 1, char );
	}
	else
		Safefree( i1->value );
}

int inifile_read_integer(
	ini_file_t *ini, const char *s_section, const char *s_ident, int i_default
) {
	const char *val = inifile_read_string( ini, s_section, s_ident, NULL );
	if( val == NULL )
		return i_default;
	return atoi( val );
}

void inifile_write_integer(
	ini_file_t *ini, const char *s_section, const char *s_ident, int i_value
) {
	char tmp[11];
	my_itoa( tmp, i_value, 10 );
	inifile_write_string( ini, s_section, s_ident, tmp );
}

double inifile_read_float(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	double d_default
) {
	const char *val = inifile_read_string( ini, s_section, s_ident, NULL );
	if( val == NULL )
		return d_default;
	return atof( val );
}

void inifile_write_float(
	ini_file_t *ini, const char *s_section, const char *s_ident, double d_value
) {
	char tmp[42];
	my_ftoa( tmp, d_value );
	inifile_write_string( ini, s_section, s_ident, tmp );
}

char inifile_read_char(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	char c_default
) {
	const char *val = inifile_read_string( ini, s_section, s_ident, NULL );
	if( val == NULL )
		return c_default;
	return val[0];
}

void inifile_write_char(
	ini_file_t *ini, const char *s_section, const char *s_ident, char c_value
) {
	char tmp[2];
	tmp[0] = c_value;
	tmp[1] = '\0';
	inifile_write_string( ini, s_section, s_ident, tmp );
}

void inifile_delete_section( ini_file_t *ini, const char *s_section ) {
	ini_section_t *s1;
	ini_item_t *i1, *i2;
	if( ini == NULL )
		return;
	for( s1 = ini->first_section; s1 != NULL; s1 = s1->next ) {
		if( my_stricmp( s1->name, s_section ) != 0 )
			continue;
		if( ini->first_section == s1 )
			ini->first_section = s1->next;
		if( ini->last_section == s1 )
			ini->last_section = s1->prev;
		if( s1->prev )
			s1->prev->next = s1->next;
		if( s1->next )
			s1->next->prev = s1->prev;
		for( i1 = s1->first_item; i1 != NULL; ) {
			i2 = i1->next;
			Safefree( i1->ident );
			Safefree( i1->value );
			Safefree( i1->p_comment );
			Safefree( i1 );
			i1 = i2;
		}
		Safefree( s1->s_comment );
		Safefree( s1->p_comment );
		Safefree( s1->name );
		Safefree( s1 );
		ini->changed = 1;
		return;
	}
}

void inifile_delete_ident(
	ini_file_t *ini, const char *s_section, const char *s_ident
) {
	ini_section_t *s1;
	ini_item_t *i1;
	if( ini == NULL )
		return;
	for( s1 = ini->first_section; s1 != NULL; s1 = s1->next ) {
		if( my_stricmp( s1->name, s_section ) != 0 )
			continue;
		for( i1 = s1->first_item; i1 != NULL; i1 = i1->next ) {
			if( my_stricmp( i1->ident, s_ident ) != 0 )
				continue;
			if( s1->first_item == i1 )
				s1->first_item = i1->next;
			if( s1->last_item == i1 )
				s1->last_item = i1->prev;
			if( i1->prev )
				i1->prev->next = i1->next;
			if( i1->next )
				i1->next->prev = i1->prev;
			Safefree( i1->ident );
			Safefree( i1->value );
			Safefree( i1->p_comment );
			Safefree( i1 );
			ini->changed = 1;
			return;
		}
	}
}
