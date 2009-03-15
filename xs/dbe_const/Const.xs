#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "dbe_const.h"

/* define inline */
#if defined _WIN32
#define INLINE __inline
#define EXTERN extern
#elif defined __GNUC__
#define INLINE inline
#define EXTERN extern inline
#else
#define INLINE
#define EXTERN
#endif

INLINE char *my_strcpy( char *dst, const char *src ) {
	for( ; *src != '\0'; *dst ++ = *src ++ );
	*dst = '\0';
	return dst;
}

MODULE = DBE::Const		PACKAGE = DBE::Const

void
export( package, ... )
	SV *package;
PREINIT:
	int i, j, not_found = 0, make_var;
	char *str, *pkg, *tmp = NULL;
	STRLEN len, pkg_len;
	long val;
	HV *stash;
	const dbe_export_tag_t *tag;
	const dbe_export_item_t *item;
PPCODE:
	pkg = SvPV( package, pkg_len );
	Newx( tmp, pkg_len + 3, char );
	Copy( pkg, tmp, pkg_len, char );
	my_strcpy( tmp + pkg_len, "::" );
	stash = gv_stashpvn( pkg, pkg_len, TRUE );
	for( i = 1; i < items; i ++ ) {
		str = SvPV( ST(i), len );
		switch( *str ) {
		case '$':
			str ++, len --;
			make_var = 1;
			break;
		case ':':
			str ++, len --;
			tag = NULL;
			for( j = 0; j < dbe_export_tags_count; j ++ ) {
				if( strcmp( dbe_export_tags[j].name, str ) == 0 ) {
					tag = dbe_export_tags + j;
					break;
				}
			}
			if( tag == NULL ) {
				Perl_croak( aTHX_ "Invalid export tag \"%s\"", str );
			}
			for( j = tag->item_start; j <= tag->item_end; j ++ ) {
				item = dbe_export_items + j;
				newCONSTSUB( stash, item->name, newSViv( (IV) item->value ) );
			}
			continue;
		case '&':
			str ++, len --;
		default:
			make_var = 0;
			break;
		}
		val = dbe_const_by_name( str, len, &not_found );
		if( not_found ) {
			Perl_croak( aTHX_ "Invalid constant \"%s\"", str );
		}
		if( make_var ) {
			Renew( tmp, pkg_len + len + 3, char );
			my_strcpy( tmp + pkg_len + 2, str );
			sv_setiv( get_sv( tmp, TRUE ), (IV) val );
		}
		else {
			newCONSTSUB( stash, str, newSViv( (IV) val ) );
		}
	}
	Safefree( tmp );
	XSRETURN_EMPTY;
