#include "dbe_def.h"

const char *HEXTAB = "0123456789abcdef";

dbe_global_t global;

/*****************************************************************************
 * driver functions
 *****************************************************************************/

INLINE void dbe_drv_set_error(
	dbe_con_t *con, int code, const char *msg, ...
) {
	va_list a;
	int i;
	va_start( a, msg );
	if( con != NULL ) {
		con->last_errno = code;
		if( msg == NULL )
			con->last_error[0] = '\0';
		else {
			i = vsnprintf(
				con->last_error, sizeof( con->last_error ), msg, a );
			/* remove trailing whitespace */
			if( i > 0 ) {
				for( i --; i >= 0 && isSPACE( con->last_error[i] ); i -- );
				con->last_error[++ i] = '\0';
			}
#ifdef DBE_DEBUG
			_debug( "set_error %d [%s]\n", code, con->last_error );
#endif
		}
	}
	else {
		global.last_errno = code;
		if( msg == NULL )
			global.last_error[0] = '\0';
		else {
			i = vsnprintf(
				global.last_error, sizeof( global.last_error ), msg, a );
			/* remove trailing whitespace */
			if( i > 0 ) {
				for( i --; i >= 0 && isSPACE( global.last_error[i] ); i -- );
				global.last_error[++ i] = '\0';
			}
#ifdef DBE_DEBUG
			_debug( "set_error %d [%s]\n", code, global.last_error );
#endif
		}
	}
	va_end( a );
}

INLINE char *dbe_drv_error_str( int errnum, char *buf, size_t buflen ) {
#if defined _WIN32
	DWORD ret;
	char *s1;
	ret = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL,
		errnum,
		LANG_USER_DEFAULT, 
		buf,
		(DWORD) buflen,
		NULL
	);
	if( ret == 0 )
		return NULL;
	for( s1 = buf + ret; s1 > buf && (*s1 == '\r' || *s1 == '\n'); s1 -- );
	*s1 = '\0';
	return buf;
#elif (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	return strerror_r( errnum, buf, buflen ) == 0 ? buf : NULL;
#else
	return strerror_r( errnum, buf, buflen );
#endif
}

INLINE void dbe_drv_trace(
	dbe_con_t *con, int level, int show_caller, const char *msg, ...
) {
	va_list va;
	size_t len;
	char *tmp = NULL, *buf, *s1, *pkg, *file;
	STRLEN file_len, pkg_len;
	SV *sv;
	int count, line;
	if( con != NULL && con->trace_local ) {
		if( con->trace_level < level || con->sv_trace_out == NULL )
			return;
	}
	else {
		if( global.trace_level < level || global.sv_trace_out == NULL )
			return;
		con = NULL;
	}
	len = strlen( msg );
	if( show_caller ) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs( sv_2mortal( newSViv( 0 ) ) );
		PUTBACK;
		count = call_pv( "DBE::__caller__", G_ARRAY );
		SPAGAIN;
		/* package, file, line */
		if( count >= 3 ) {
			sv = POPs;
			pkg = SvPVx( sv, pkg_len );
			sv = POPs;
			file = SvPVx( sv, file_len );
			line = (int) POPi;
			Newx( tmp, len + 32 + file_len, char );
			sprintf( tmp, "    %s at %s line %d\n", msg, file, line );
		}
		PUTBACK;
		FREETMPS;
		LEAVE;
	}
	if( tmp == NULL ) {
		Newx( tmp, len + 32, char );
		s1 = tmp;
		*s1 ++ = ' ', *s1 ++ = ' ', *s1 ++ = ' ', *s1 ++ = ' ';
		s1 = my_strcpy( s1, msg );
		*s1 ++ = '\n', *s1 = '\0';
	}
	va_start( va, msg );
	Newx( buf, len + 4096, char );
	len = vsnprintf( buf, len + 4096, tmp, va );
	va_end( va );
	if( con != NULL ) {
		dbe_perl_print( con->sv_trace_out, buf, len );
	}
	else {
		dbe_perl_print( global.sv_trace_out, buf, len );
	}
	Safefree( tmp );
	Safefree( buf );
}

void dbe_drv_set_base_clase(
	dbe_drv_t *drv, enum dbe_object obj, const char *classname
) {
	size_t len = strlen( classname );
	switch( obj ) {
	case DBE_OBJECT_CON:
#ifdef DBE_DEBUG
		_debug( "[%s] extend connection class to [%s]\n", drv->name, classname );
#endif
		Renew( drv->class_con, len + 1, char );
		Copy( classname, drv->class_con, len + 1, char );
		break;
	case DBE_OBJECT_RES:
#ifdef DBE_DEBUG
		_debug( "[%s] extend result class to [%s]\n", drv->name, classname );
#endif
		Renew( drv->class_res, len + 1, char );
		Copy( classname, drv->class_res, len + 1, char );
		break;
	case DBE_OBJECT_STMT:
#ifdef DBE_DEBUG
		_debug( "[%s] extend statement class to [%s]\n", drv->name, classname );
#endif
		Renew( drv->class_stmt, len + 1, char );
		Copy( classname, drv->class_stmt, len + 1, char );
		break;
	default:
		break;
	}
}

void *dbe_drv_get_object( enum dbe_object obj, SV *this ) {
	dbe_con_t *con;
	dbe_res_t *res;
	dbe_stmt_t *stmt;
	switch( obj ) {
	case DBE_OBJECT_CON:
		con = dbe_con_find( this );
		return con == NULL ? NULL : con->drv_data;
	case DBE_OBJECT_RES:
		res = dbe_res_find( this );
		return res == NULL || res->type == RES_TYPE_VRT ? NULL : res->drv_data;
	case DBE_OBJECT_STMT:
		stmt = dbe_stmt_find( this );
		return stmt == NULL ? NULL : stmt->drv_data;
	default:
		break;
	}
	return NULL;
}

void dbe_drv_handle_error_xs(
	dbe_con_t *con, enum dbe_object obj, SV *this, const char *action
) {
	const char *obj_id;
	dXSARGS;
	PERL_UNUSED_VAR(items); /* -W */
	switch( obj ) {
	case DBE_OBJECT_CON:
		obj_id = "con";
		break;
	case DBE_OBJECT_RES:
		obj_id = "res";
		break;
	case DBE_OBJECT_STMT:
		obj_id = "stmt";
		break;
	case DBE_OBJECT_LOB:
		obj_id = "lob";
		break;
	default:
		obj_id = "";
		break;
	}
	if( action == NULL )
		action = "Database error";
	HANDLE_ERROR_XS( this, obj_id, con, action );
}

dbe_res_t *dbe_drv_vrt_create_result( dbe_con_t *con ) {
	dbe_res_t *res;
	dbe_vrt_res_t *vres;
	if( con == NULL )
		return NULL;
	Newxz( res, 1, dbe_res_t );
	Newxz( vres, 1, dbe_vrt_res_t );
	res->type = RES_TYPE_VRT;
	res->con = con;
	res->vrt_res = vres;
	return res;
}

void dbe_drv_vrt_free_result( dbe_res_t *res ) {
	if( res->type == RES_TYPE_VRT && ! res->id )
		dbe_res_free_vrt( res );
}

int dbe_drv_vrt_add_col(
	dbe_res_t *res, const char *name, size_t name_length, enum dbe_type type
) {
	dbe_vrt_res_t *vres;
	dbe_vrt_column_t *vcol;
	if( res->type != RES_TYPE_VRT ) {
		dbe_drv_set_error( res->con, -1, "Not a valid recordset" );
		return DBE_ERROR;
	}
	vres = res->vrt_res;
	if( (vres->column_count % 8) == 0 ) {
		u_long cc = vres->column_count + 8;
		Renew( vres->columns, cc, dbe_vrt_column_t );
		Renew( res->sv_bind, cc, SV * );
		Zero( res->sv_bind, cc, SV * );
	}
	res->num_fields ++;
	vcol = vres->columns + vres->column_count ++;
	vcol->type = type;
	vcol->name_length = name_length;
	Newx( vcol->name, name_length + 1, char );
	Copy( name, vcol->name, name_length + 1, char );
	if( res->con->flags & CON_NAME_LC ) {
		int len;
		vcol->name_conv_length = name_length * 2;
		Newx( vcol->name_conv, vcol->name_conv_length + 1, char );
		len = my_utf8_tolower(
			vcol->name, vcol->name_length,
			vcol->name_conv, vcol->name_conv_length
		);
		if( len < 0 ) {
			/* invalid utf-8, should not happen */
			char *s1, *s2, *se;
			s1 = vcol->name, s2 = vcol->name_conv, se = s1 + name_length;
			for( ; s1 < se; *s2 ++ = tolower( *s1 ++ ) );
			*s2 = '\0';
			vcol->name_conv_length = name_length;
		}
		else {
			vcol->name_conv_length = len;
		}
		/* truncate to real length */
		Renew( vcol->name_conv, vcol->name_conv_length + 1, char );
	}
	else if( res->con->flags & CON_NAME_UC ) {
		int len;
		vcol->name_conv_length = name_length * 2;
		Newx( vcol->name_conv, vcol->name_conv_length + 1, char );
		len = my_utf8_toupper(
			vcol->name, vcol->name_length,
			vcol->name_conv, vcol->name_conv_length
		);
		if( len < 0 ) {
			/* invalid utf-8, should not happen */
			char *s1, *s2, *se;
			s1 = vcol->name, s2 = vcol->name_conv, se = s1 + name_length;
			for( ; s1 < se; *s2 ++ = toupper( *s1 ++ ) );
			*s2 = '\0';
			vcol->name_conv_length = name_length;
		}
		else {
			vcol->name_conv_length = len;
		}
		/* truncate to real length */
		Renew( vcol->name_conv, vcol->name_conv_length + 1, char );
	}
	else {
		vcol->name_conv = vcol->name;
		vcol->name_conv_length = name_length;
	}
	return DBE_OK;
}

int dbe_drv_vrt_add_row( dbe_res_t *res, dbe_str_t *data ) {
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	size_t i;
	if( res->type != RES_TYPE_VRT ) {
		dbe_drv_set_error( res->con, -1, "Not a valid recordset" );
		return DBE_ERROR;
	}
	vres = res->vrt_res;
	if( ! vres->column_count ) {
		dbe_drv_set_error( res->con,
			-1, "Recordset must have at least one column" );
		return DBE_ERROR;
	}
	if( (vres->row_count % 8) == 0 ) {
		Renew( vres->rows, vres->row_count + 8, dbe_vrt_row_t );
	}
	vrow = &vres->rows[vres->row_count ++];
	Newxz( vrow->data, vres->column_count, char * );
	Newxz( vrow->lengths, vres->column_count, size_t );
	for( i = 0; i < vres->column_count; i ++ ) {
		if( data[i].value == NULL )
			continue;
		switch( vres->columns[i].type ) {
		case DBE_TYPE_VARCHAR:
		default:
			Newx( vrow->data[i], data[i].length + 1, char );
			Copy( data[i].value, vrow->data[i], data[i].length, char );
			vrow->data[i][data[i].length] = '\0';
			vrow->lengths[i] = data[i].length;
			break;
		case DBE_TYPE_BINARY:
			Newx( vrow->data[i], data[i].length, char );
			Copy( data[i].value, vrow->data[i], data[i].length, char );
			vrow->lengths[i] = data[i].length;
			break;
		case DBE_TYPE_INTEGER:
			Newx( vrow->data[i], sizeof( int ), char );
			*((int *) vrow->data[i]) = *((int *) data[i].value);
			vrow->lengths[i] = sizeof( int );
			break;
		case DBE_TYPE_BIGINT:
			Newx( vrow->data[i], sizeof( xlong ), char );
			*((xlong *) vrow->data[i]) = *((xlong *) data[i].value);
			vrow->lengths[i] = sizeof( xlong );
			break;
		case DBE_TYPE_DOUBLE:
			Newx( vrow->data[i], sizeof( double ), char );
			*((double *) vrow->data[i]) = *((double *) data[i].value);
			vrow->lengths[i] = sizeof( double );
			break;
		}
	}
	return DBE_OK;
}

dbe_res_t *dbe_drv_alloc_result(
	dbe_con_t *dbe_con, drv_res_t *drv_res, u_long num_fields
) {
	dbe_res_t *res;
	if( ! num_fields ) {
		dbe_drv_set_error( dbe_con, -1, NOFIELDS_ERROR );
		HANDLE_ERROR( dbe_con->this, "con", dbe_con, "Allocate result" );
		return NULL;
	}
	Newxz( res, 1, dbe_res_t );
	res->type = RES_TYPE_DRV;
	res->con = dbe_con;
	res->drv_data = drv_res;
	res->num_fields = num_fields;
	Newxz( res->sv_buffer, num_fields, SV * );
	Newxz( res->sv_bind, num_fields, SV * );
	return res;
}

void dbe_drv_free_result( dbe_res_t *res ) {
	if( res->type == RES_TYPE_DRV && ! res->id )
		dbe_res_free_drv( res );
}

regexp *dbe_drv_regexp_from_string( const char *str, size_t str_len ) {
	char end = *str;
	const char *sp, *se;
	int flags = 0;
#ifdef DBE_DEBUG
	_debug( "prepare regexp for [%s]\n", str );
#endif
	if( str_len == 0 )
		str_len = strlen( str );
	se = str + str_len - 1;
	for( sp = se; *sp != end; sp -- ) {
		if( sp == str )
			return NULL;
	}
	if( sp == str )
		return NULL;
	for( ; se > sp; se -- ) {
		switch( *se ) {
			case 'e': flags |= PMf_EVAL;		break;
			case 'g': flags |= PMf_GLOBAL;		break;
			case 'i': flags |= PMf_FOLD;		break;
			case 'm': flags |= PMf_MULTILINE;	break;
			case 's': flags |= PMf_SINGLELINE;	break;
			case 'x': flags |= PMf_EXTENDED;	break;
		}
	}
#ifdef DBE_DEBUG
	_debug( "create regexp for [%s] flags %d\n", str, flags );
#endif
#if PERL_REVISION > 5 || (PERL_REVISION == 5 && PERL_VERSION >= 10)
	return Perl_pregcomp(
		aTHX_ sv_2mortal( newSVpvn( str + 1, sp - str - 1 ) ), flags );
#else
	{
		char *s = (char *) str;
		PMOP pm;
		Zero( &pm, 1, PMOP );
		pm.op_pmflags = flags;
		return Perl_pregcomp( aTHX_ s + 1, (char *) sp - 1, &pm );
	}
#endif
}

#define DBE_FILTER_FLAGS	PMf_SINGLELINE | PMf_FOLD

regexp *dbe_drv_regexp_from_pattern(
	const char *pattern, size_t pattern_len, int force
) {
	const char *p1;
	char *tmp, *p2;
	int esc = 0, f = 0;
	regexp *re;
	if( pattern_len == 0 )
		pattern_len = strlen( pattern );
	Newx( tmp, pattern_len * 3 + 4, char );
	p2 = tmp;
	*p2 ++ = '^';
	for( p1 = pattern; *p1 != '\0'; p1 ++ ) {
		switch( *p1 ) {
		case '\\':
			if( esc )
				*p2 ++ = '\\';
			esc = ! esc;
			break;
		case '_':
			if( esc ) {
				*p2 ++ = '\\';
				*p2 ++ = '_';
			}
			else {
				*p2 ++ = '.';
				f |= 1;
			}
			break;
		case '%':
			if( esc ) {
				*p2 ++ = '\\';
				*p2 ++ = '%';
			}
			else {
				*p2 ++ = '.';
				*p2 ++ = '*';
				*p2 ++ = '?';
				f |= 2;
			}
			break;
		case '[':
		case ']':
		case '$':
		case '^':
		case '*':
			if( esc ) {
				*p2 ++ = '\\';
			}
			else {
				f |= 4;
			}
			*p2 ++ = *p1;
			break;
		case '.':
			if( esc ) {
				*p2 ++ = '.';
			}
			else {
				*p2 ++ = '\\';
				*p2 ++ = '.';
			}
			break;
		default:
			*p2 ++ = *p1;
			break;
		}
	}
	*p2 ++ = '$';
	*p2 = '\0';
	if( f || force ) {
#ifdef DBE_DEBUG
		_debug( "create regexp [%s] from pattern [%s]\n", tmp, pattern );
#endif
#if PERL_REVISION > 5 || (PERL_REVISION == 5 && PERL_VERSION >= 10)
		re = Perl_pregcomp(
			aTHX_ sv_2mortal( newSVpvn( tmp, p2 - tmp ) ), DBE_FILTER_FLAGS );
#else
		{
			PMOP pm;
			Zero( &pm, 1, PMOP );
			pm.op_pmflags = DBE_FILTER_FLAGS;
			re = Perl_pregcomp( aTHX_ tmp, p2 - 1, &pm );
		}
#endif
	}
	else
		re = NULL;
	Safefree( tmp );
	return re;
}

int dbe_drv_regexp_match( regexp *re, const char *str, size_t str_len ) {
	if( re == NULL )
		return 1;
	if( str == NULL )
		return 0;
	if( str_len == 0 )
		str_len = strlen( str );
	return Perl_pregexec(
		aTHX_ re,
		(char *) str, (char *) str + str_len, (char *) str,
		0, &PL_sv_undef, 1
	);
}

void dbe_drv_regexp_free( regexp *re ) {
	if( re != NULL ) {
#ifdef DBE_DEBUG
		_debug( "free regexp 0x%lx\n", (size_t) re );
#endif
		Perl_pregfree( aTHX_ re );
	}
}

size_t dbe_drv_unescape_pattern( char *str, size_t str_len, const char pesc ) {
	char *wp = str, *rp = str, *ep;
	int esc = 0;
	unsigned int wc;
	if( str_len == 0 )
		str_len = strlen( str );
	for( ep = str + str_len; rp < ep; rp ++, wp ++ ) {
		wc = (unsigned char) *rp;
		/* skip utf-8 characters */
		if( wc & 0x80 ) {
			*wp ++ = (char) wc;
			wc &= 0x3F;
			if( wc & 0x20 ) {
				if( ++ rp == ep )
					break;
				*wp ++ = *rp;
			}
			if( ++ rp == ep )
				break;
			*wp = *rp;
			continue;
		}
		if( ! esc ) {
			if( wc == pesc ) {
				esc = 1;
				continue;
			}
		}
		else {
			switch( wc ) {
			case '_':
			case '%':
			case '[':
			case ']':
			case '^':
			case '$':
			case '!':
			case '*':
				wp --;
				break;
			default:
				*wp ++ = pesc;
				break;
			}
			esc = 0;
		}
		*wp = (char) wc;
	}
	*wp = '\0';
	return wp - str;
}

SV *dbe_drv_lob_stream_alloc( dbe_con_t *con, void *context ) {
	dbe_lob_stream_t *ls;
	Newxz( ls, 1, dbe_lob_stream_t );
	ls->context = context;
	ls->size = -1;
	GLOBAL_LOCK();
	ls->id = ++ global.counter;
	GLOBAL_UNLOCK();
	CON_LOCK( con );
	if( con->first_lob == NULL ) {
		con->first_lob = ls;
	}
	else {
		ls->prev = con->last_lob;
		con->last_lob->next = ls;
	}
	con->last_lob = ls;
	CON_UNLOCK( con );
#ifdef DBE_DEBUG
	_debug( "alloc lob stream %u with context 0x%lx\n", ls->id, context );
#endif

	return sv_setref_iv( newSV( 0 ), "DBE::LOB", (IV) ls->id );
}

void dbe_drv_lob_stream_free( dbe_con_t *con, void *context ) {
	dbe_lob_stream_t *ls;
	CON_LOCK( con );
	for( ls = con->first_lob; ls != NULL; ls = ls->next ) {
		if( ls->context == context )
			goto _found;
	}
	CON_UNLOCK( con );
	return;
_found:
	CON_UNLOCK( con );
	dbe_lob_stream_rem( con, ls );
}

void *dbe_drv_lob_stream_get( dbe_con_t *con, SV *sv ) {
	u_long id;
	dbe_lob_stream_t *ls;
	if( SvROK( sv ) )
		sv = SvRV( sv );
	if( ! SvIOK( sv ) )
		return NULL;
	id = (u_long) SvIV( sv );
	CON_LOCK( con );
	for( ls = con->last_lob; ls != NULL; ls = ls->prev ) {
		if( ls->id == id )
			goto _found;
	}
	CON_UNLOCK( con );
	return NULL;
_found:
	CON_UNLOCK( con );
	return ls->context;
}

/*
_g_dbe ()
*/

dbe_t g_dbe = {
	DBE_VERSION,
	dbe_drv_set_error,
	dbe_drv_trace,
	dbe_drv_set_base_clase,
	dbe_drv_get_object,
	dbe_drv_handle_error_xs,
	dbe_drv_error_str,
	dbe_drv_alloc_result,
	dbe_drv_free_result,
	dbe_drv_vrt_create_result,
	dbe_drv_vrt_free_result,
	dbe_drv_vrt_add_col,
	dbe_drv_vrt_add_row,
	dbe_drv_regexp_from_string,
	dbe_drv_regexp_from_pattern,
	dbe_drv_regexp_match,
	dbe_drv_regexp_free,
	dbe_drv_unescape_pattern,
	dbe_escape_pattern,
	my_towupper,
	my_towlower,
	my_utf8_to_unicode,
	my_unicode_to_utf8,
	my_utf8_toupper,
	my_utf8_tolower,
	dbe_drv_lob_stream_alloc,
	dbe_drv_lob_stream_free,
	dbe_drv_lob_stream_get,
};


/*****************************************************************************
 * statement functions
 *****************************************************************************/

INLINE void dbe_stmt_add( dbe_con_t *con, dbe_stmt_t *stmt ) {
	GLOBAL_LOCK();
	stmt->id = ++ global.counter;
	GLOBAL_UNLOCK();
	CON_LOCK( con );
#ifdef DBE_DEBUG
	_debug( "add stmt %lu 0x%lx to con %lu 0x%lx\n",
		stmt->id, stmt, con->id, con );
#endif
	if( con->first_stmt == NULL )
		con->first_stmt = stmt;
	else {
		con->last_stmt->next = stmt;
		stmt->prev = con->last_stmt;
	}
	con->last_stmt = stmt;
	CON_UNLOCK( con );
}

INLINE void dbe_stmt_free( dbe_stmt_t *stmt ) {
	int i;
	dbe_param_t *param;
	drv_def_t *dd = stmt->con->drv->def;
#ifdef DBE_DEBUG
	_debug( "free stmt %lu 0x%lx\n", stmt->id, stmt );
#endif
	if( dd->stmt_free != NULL )
		dd->stmt_free( stmt->drv_data );
	if( (param = stmt->params) != NULL ) {
		for( i = 0; i < stmt->num_params; i ++, param ++ ) {
			/* release bound variables */
			if( param->sv != NULL ) {
#if (DBE_DEBUG > 1)
				_debug( "sv 0x%lx refcnt %d\n",
					param->sv, SvREFCNT( param->sv ) );
#endif
				SvREFCNT_dec( param->sv );
			}
			Safefree( param->name );
		}
		Safefree( stmt->params );
		Safefree( stmt->param_names );
	}
	Safefree( stmt );
}

INLINE void dbe_stmt_rem( dbe_stmt_t *stmt ) {
	CON_LOCK( stmt->con );
	if( stmt->con->first_stmt == stmt )
		stmt->con->first_stmt = stmt->next;
	if( stmt->con->last_stmt == stmt )
		stmt->con->last_stmt = stmt->prev;
	if( stmt->next != NULL )
		stmt->next->prev = stmt->prev;
	if( stmt->prev != NULL )
		stmt->prev->next = stmt->next;
	CON_UNLOCK( stmt->con );
	dbe_stmt_free( stmt );
}

INLINE dbe_stmt_t *dbe_stmt_find( SV *sv ) {
	dbe_con_t *con;
	dbe_stmt_t *stmt;
	u_long id;
	if( global.destroyed )
		return NULL;
	if( ! SvROK( sv ) || ! ( sv = SvRV( sv ) ) || ! SvIOK( sv ) )
		return NULL;
	id = (u_long) SvIV( sv );
	GLOBAL_LOCK();
	for( con = global.last_con; con != NULL; con = con->prev ) {
		for( stmt = con->last_stmt; stmt != NULL; stmt = stmt->prev )
			if( stmt->id == id ) {
				goto _found;
			}
	}
#ifdef DBE_DEBUG
	_debug( "stmt %lu not found\n", id );
#endif
	GLOBAL_UNLOCK();
	return NULL;
_found:
	GLOBAL_UNLOCK();
	return stmt;
}

/*****************************************************************************
 * result set functions
 *****************************************************************************/

INLINE void dbe_res_add( dbe_con_t *con, dbe_res_t *res ) {
	GLOBAL_LOCK();
	res->id = ++ global.counter;
	GLOBAL_UNLOCK();
	CON_LOCK( con );
#ifdef DBE_DEBUG
	_debug( "add res %lu (%d) 0x%lx to con %lu 0x%lx\n",
		res->id, res->type, res, con->id, con );
#endif
	if( con->first_res == NULL )
		con->first_res = res;
	else {
		con->last_res->next = res;
		res->prev = con->last_res;
	}
	con->last_res = res;
	CON_UNLOCK( con );
}

INLINE dbe_res_t *dbe_res_find( SV *sv ) {
	dbe_con_t *con;
	dbe_res_t *rf, *rl;
	u_long id;
	if( global.destroyed )
		return NULL;
	if( ! SvROK( sv ) || ! (sv = SvRV( sv )) || ! SvIOK( sv ) )
		return NULL;
	id = (u_long) SvIV( sv );
	GLOBAL_LOCK();
	for( con = global.last_con; con != NULL; con = con->prev ) {
		rf = con->first_res;
		rl = con->last_res;
		while( TRUE ) {
			if( rl == NULL )
				break;
			if( rl->id == id )
				goto retl;
			else if( rf->id == id )
				goto retf;
			rl = rl->prev;
			rf = rf->next;
		}
	}
#ifdef DBE_DEBUG
	_debug( "res %lu not found\n", id );
#endif
	GLOBAL_UNLOCK();
	return NULL;
retl:
	GLOBAL_UNLOCK();
	return rl;
retf:
	GLOBAL_UNLOCK();
	return rf;
}

INLINE void dbe_res_free_vrt( dbe_res_t *res ) {
	dbe_vrt_res_t *vres;
	size_t i, j, cc;
	vres = (dbe_vrt_res_t *) res->drv_data;
	cc = vres->column_count;
	for( i = 0; i < vres->row_count; i ++ ) {
		for( j = 0; j < cc; j ++ )
			Safefree( vres->rows[i].data[j] );
		Safefree( vres->rows[i].data );
		Safefree( vres->rows[i].lengths );
	}
	Safefree( vres->rows );
	for( j = 0; j < cc; j ++ ) {
		if( vres->columns[j].name_conv != vres->columns[j].name )
			Safefree( vres->columns[j].name_conv );
		Safefree( vres->columns[j].name );
		/* release bound variables */
		if( res->sv_bind[j] != NULL )
			SvREFCNT_dec( res->sv_bind[j] );
	}
	Safefree( vres->columns );
	Safefree( vres );
	Safefree( res->sv_bind );
	Safefree( res );
}

INLINE void dbe_res_free_drv( dbe_res_t *res ) {
	drv_def_t *dd;
	u_long i;
	dd = res->con->drv->def;
	if( dd->res_free != NULL )
		dd->res_free( res->drv_data );
	Safefree( res->sv_buffer );
	for( i = 0; i < res->num_fields; i ++ ) {
		/* release bound variables */
		if( res->sv_bind[i] != NULL ) {
#if (DBE_DEBUG > 1)
			_debug( "sv 0x%lx refcnt %d\n",
				res->sv_bind[i], SvREFCNT( res->sv_bind[i] ) );
#endif
			SvREFCNT_dec( res->sv_bind[i] );
		}
	}
	Safefree( res->sv_bind );
	if( res->flags & RES_FREE_NAMES ) {
#ifdef DBE_DEBUG
		_debug( "free field names allocated by the driver\n" );
#endif
		if( dd->mem_free != NULL ) {
			for( i = 0; i < res->num_fields; i ++ )
				dd->mem_free( res->names[i].value );
		}
		else {
			for( i = 0; i < res->num_fields; i ++ )
				Safefree( res->names[i].value );
		}
	}
	if( res->names_conv != res->names ) {
		for( i = 0; i < res->num_fields; i ++ )
			Safefree( res->names_conv[i].value );
		Safefree( res->names_conv );
	}
	Safefree( res->names );
	Safefree( res );
}

INLINE void dbe_res_free( dbe_res_t *res ) {
#ifdef DBE_DEBUG
	_debug( "free res %lu (%d) 0x%lx\n", res->id, res->type, (size_t) res );
#endif
	if( res->type == RES_TYPE_DRV )
		dbe_res_free_drv( res );
	else
		dbe_res_free_vrt( res );
}

INLINE void dbe_res_rem( dbe_res_t *res ) {
	CON_LOCK( res->con );
	if( res->con->first_res == res )
		res->con->first_res = res->next;
	if( res->con->last_res == res )
		res->con->last_res = res->prev;
	if( res->next != NULL )
		res->next->prev = res->prev;
	if( res->prev != NULL )
		res->prev->next = res->next;
	CON_UNLOCK( res->con );
	dbe_res_free( res );
}

INLINE int dbe_res_fetch_names( SV *this, dbe_res_t *res ) {
	drv_def_t *dd;
	int flags = 0, r;
	if( res->names != NULL )
		return DBE_OK;
	dd = res->con->drv->def;
	if( dd->res_fetch_names == NULL )
		NOFUNCTION_C( this, "res", res->con, "res_fetch_names" );
	Newx( res->names, res->num_fields, dbe_str_t );
	r = dd->res_fetch_names( res->drv_data, res->names, &flags );
	if( r != DBE_OK )
		return r;
	if( flags & DBE_FREE_ITEMS )
		res->flags |= RES_FREE_NAMES;
	if( res->con->flags & CON_NAME_LC ) {
		dbe_str_t *n1, *n2, *ne;
		int len;
		Newx( res->names_conv, res->num_fields, dbe_str_t );
		n1 = res->names, n2 = res->names_conv, ne = n1 + res->num_fields;
		for( ; n1 < ne; n1 ++, n2 ++ ) {
			n2->length = n1->length * 2;
			Newx( n2->value, n2->length + 1, char );
			len = my_utf8_tolower(
				n1->value, n1->length, n2->value, n2->length );
			if( len < 0 ) {
				char *s1 = n1->value, *s2 = n2->value, *se = s1 + n1->length;
				for( ; s1 < se; *s2 ++ = tolower( *s1 ++ ) );
				*s2 = '\0';
				n2->length = n1->length;
			}
			else {
				n2->length = len;
			}
			/* truncate to real length */
			Renew( n2->value, n2->length + 1, char );
		}
	}
	else if( res->con->flags & CON_NAME_UC ) {
		dbe_str_t *n1, *n2, *ne;
		int len;
		Newx( res->names_conv, res->num_fields, dbe_str_t );
		n1 = res->names, n2 = res->names_conv, ne = n1 + res->num_fields;
		for( ; n1 < ne; n1 ++, n2 ++ ) {
			n2->length = n1->length * 2;
			Newx( n2->value, n2->length + 1, char );
			len = my_utf8_toupper(
				n1->value, n1->length, n2->value, n2->length );
			if( len < 0 ) {
				char *s1 = n1->value, *s2 = n2->value, *se = s1 + n1->length;
				for( ; s1 < se; *s2 ++ = toupper( *s1 ++ ) );
				*s2 = '\0';
				n2->length = n1->length;
			}
			else {
				n2->length = len;
			}
			/* truncate to real length */
			Renew( n2->value, n2->length + 1, char );
		}
	}
	else {
		res->names_conv = res->names;
	}
	return DBE_OK;
}

INLINE int dbe_res_fetch_row( dbe_res_t *res, int names ) {
	drv_def_t *dd = res->con->drv->def;
	int r;
	u_long i;
	Zero( res->sv_buffer, res->num_fields, SV * );
	r = dd->res_fetch_row( res->drv_data, res->sv_buffer, names );
	if( r != DBE_OK && r != DBE_NO_DATA ) {
		for( i = 0; i < res->num_fields; i ++ ) {
			if( res->sv_buffer[i] != NULL )
				sv_2mortal( res->sv_buffer[i] );
		}
	}
	return r;
}

INLINE int dbe_res_fetch_hash( SV *this, dbe_res_t *res, HV **phv, int nc ) {
	drv_def_t *dd = res->con->drv->def;
	HV *hv;
	u_long i;
	char *name, *p1;
	STRLEN name_len;
	SV *sv;
	if( dd->res_fetch_row == NULL )
		NOFUNCTION_C( this, "res", res->con, "fetch_row" );
	if( res->names == NULL )
		if( dbe_res_fetch_names( this, res ) != DBE_OK )
			return DBE_ERROR;
	if( dbe_res_fetch_row( res, TRUE ) != DBE_OK )
		return DBE_NO_DATA;
	if( (*phv) == NULL )
		(*phv) = hv = (HV *) sv_2mortal( (SV *) newHV() );
	else
		hv = (*phv);
	switch( nc ) {
	case 1:
		name = NULL;
		name_len = 0;
		for( i = 0; i < res->num_fields; i ++ ) {
			if( name_len < res->names[i].length ) {
				name_len = res->names[i].length;
				Renew( name, name_len + 1, char );
			}
			p1 = my_strcpyl( name, res->names[i].value );
			sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store( hv, name, (I32) (p1 - name), sv, 0 );
		}
		Safefree( name );
		break;
	case 2:
		name = NULL;
		name_len = 0;
		for( i = 0; i < res->num_fields; i ++ ) {
			if( name_len < res->names[i].length ) {
				name_len = res->names[i].length;
				Renew( name, name_len + 1, char );
			}
			p1 = my_strcpyu( name, res->names[i].value );
			sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store( hv, name, (I32) (p1 - name), sv, 0 );
		}
		Safefree( name );
		break;
	default:
		for( i = 0; i < res->num_fields; i ++ ) {
			sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store(
				hv, res->names_conv[i].value, (I32) res->names_conv[i].length,
				sv, 0
			);
		}
		break;
	}
	return DBE_OK;
}

INLINE int dbe_res_fetchall_arrayref(
	SV *this, dbe_res_t *res, AV **pav, int fn
) {
	drv_def_t *dd = res->con->drv->def;
	SV **row = res->sv_buffer;
	u_long i;
	AV *av, *av2;
	HV *hv;
	SV *sv;
	if( dd->res_fetch_row == NULL )
		NOFUNCTION_C( this, "res", res->con, "fetch_row" );
	if( fn ) {
		if( res->names == NULL )
			if( dbe_res_fetch_names( this, res ) != DBE_OK )
				return DBE_ERROR;
		(*pav) = av = (AV *) sv_2mortal( (SV *) newAV() );
		while( dbe_res_fetch_row( res, TRUE ) == DBE_OK ) {
			hv = (HV *) sv_2mortal( (SV *) newHV() );
			for( i = 0; i < res->num_fields; i ++ ) {
				sv = row[i] != NULL ? row[i] : newSV(0);
				(void) hv_store(
					hv, res->names_conv[i].value,
					(I32) res->names_conv[i].length,
					sv, 0
				);
			}
			av_push( av, newRV( (SV *) hv ) );
		}
	}
	else {
		(*pav) = av = (AV *) sv_2mortal( (SV *) newAV() );
		while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
			av2 = (AV *) sv_2mortal( (SV *) newAV() );
			for( i = 0; i < res->num_fields; i ++ ) {
				if( row[i] != NULL )
					av_push( av2, row[i] );
				else
					av_push( av2, newSV(0) );
			}
			av_push( av, newRV( (SV *) av2 ) );
		}
	}
	return DBE_OK;
}

INLINE int dbe_res_fetchall_hashref(
	SV *this, dbe_res_t *res, dbe_str_t *fields, u_long ks, HV **phv
) {
	drv_def_t *dd = res->con->drv->def;
	dbe_str_t *names;
	SV **row = res->sv_buffer;
	int *index;
	u_long i, j;
	HV *root, *hv, *hv2;
	SV **psv;
	char *skey;
	STRLEN lkey;
	if( dd->res_fetch_row == NULL )
		NOFUNCTION_C( this, "res", res->con, "fetch_row" );
	if( res->names == NULL )
		if( dbe_res_fetch_names( this, res ) != DBE_OK )
			return DBE_ERROR;
	names = res->names_conv;
	Newx( index, ks, int );
	for( i = 0; i < ks; i ++ ) {
		index[i] = -1;
		for( j = 0; j < res->num_fields; j ++ ) {
			if( strcmp( fields[i].value, names[j].value ) == 0 ) {
				index[i] = (int) j;
				break;
			}
		}
		if( index[i] < 0 ) {
			dbe_drv_set_error( res->con, -1,
				"Field '%s' does not exist", fields[i].value );
			goto _error;
		}
	}
	(*phv) = root = (HV *) sv_2mortal( (SV *) newHV() );
	while( dbe_res_fetch_row( res, TRUE ) == DBE_OK ) {
		for( hv = root, i = 0; i < ks; i ++ ) {
			if( row[index[i]] != NULL )
				skey = SvPVx( row[index[i]], lkey );
			else {
				skey = "";
				lkey = 0;
			}
			psv = hv_fetch( hv, skey, (I32) lkey, 0 );
			if( psv != NULL )
				hv = (HV *) SvRV( (*psv) );
			else {
				hv2 = (HV *) sv_2mortal( (SV *) newHV() );
				(void) hv_store( hv,
					skey, (I32) lkey, newRV( (SV *) hv2 ), 0 );
				hv = hv2;
			}
		}
		for( i = 0; i < res->num_fields; i ++ ) {
			if( row[i] == NULL )
				row[i] = newSV(0);
			(void) hv_store( hv,
				names[i].value, (I32) names[i].length, row[i], 0 );
		}
	}
	Safefree( index );
	return DBE_OK;
_error:
	Safefree( index );
	return DBE_ERROR;
}

/*****************************************************************************
 * connection functions
 *****************************************************************************/

INLINE void dbe_con_add( dbe_con_t *con ) {
	GLOBAL_LOCK();
	con->id = ++ global.counter;
#ifdef DBE_DEBUG
	_debug( "add con %lu 0x%lx\n", con->id, con );
#endif
	if( global.first_con == NULL )
		global.first_con = con;
	else {
		con->prev = global.last_con;
		global.last_con->next = con;
	}
	global.last_con = con;
	if( ! global.cleanup ) {
		dSP;
		PUSHMARK( SP );
		call_pv( "DBE::__reg_cleanup__", G_DISCARD | G_NOARGS );
		global.cleanup = 1;
	}
	GLOBAL_UNLOCK();
}

INLINE void dbe_con_cleanup( dbe_con_t *con ) {
	dbe_res_t *r1, *r2;
	dbe_stmt_t *s1, *s2;
	dbe_lob_stream_t *ls1, *ls2;
	r1 = con->first_res;
	while( r1 != NULL ) {
		r2 = r1->next;
		dbe_res_free( r1 );
		r1 = r2;
	}
	con->first_res = con->last_res = NULL;
	s1 = con->first_stmt;
	while( s1 != NULL ) {
		s2 = s1->next;
		dbe_stmt_free( s1 );
		s1 = s2;
	}
	con->first_stmt = con->last_stmt = NULL;
	ls1 = con->first_lob;
	while( ls1 != NULL ) {
		ls2 = ls1->next;
		dbe_lob_stream_free( con, ls1 );
		ls1 = ls2;
	}
	con->first_lob = con->last_lob = NULL;
}

INLINE void dbe_con_free( dbe_con_t *con ) {
#ifdef DBE_DEBUG
	_debug( "free con %lu 0x%lx\n", con->id, con );
#endif
	dbe_con_cleanup( con );
	if( con->drv->def->con_free != NULL ) {
		con->drv->def->con_free( con->drv_data );
	}
	if( con->error_handler != NULL ) {
		sv_2mortal( con->error_handler );
		if( con->error_handler_arg != NULL )
			sv_2mortal( con->error_handler_arg );
	}
	if( con->sv_trace_out != NULL )
		SvREFCNT_dec( con->sv_trace_out );
#ifdef USE_ITHREADS
	MUTEX_DESTROY( &con->thread_lock );
#endif
	Safefree( con );
}

INLINE void dbe_con_rem( dbe_con_t *con ) {
	GLOBAL_LOCK();
	if( con->prev )
		con->prev->next = con->next;
	else if( con == global.first_con )
		global.first_con = con->next;
	if( con->next )
		con->next->prev = con->prev;
	else if( con == global.last_con )
		global.last_con = con->prev;
	GLOBAL_UNLOCK();
	dbe_con_free( con );
}

INLINE dbe_con_t *dbe_con_find( SV *sv ) {
	if( ! SvROK( sv ) || ! (sv = SvRV( sv )) || ! SvIOK( sv ) )
		return NULL;
	return dbe_con_find_by_id( sv, (u_long) SvIV( sv ) );
}

INLINE dbe_con_t *dbe_con_find_by_id( SV *sv, u_long id ) {
	dbe_con_t *cf, *cl;
	if( global.destroyed )
		return NULL;
	GLOBAL_LOCK();
	cf = global.first_con;
	cl = global.last_con;
	while( TRUE ) {
		if( cl == NULL )
			break;
		if( cl->id == id )
			goto retl;
		else if( cf->id == id )
			goto retf;
		cl = cl->prev;
		cf = cf->next;
	}
#ifdef DBE_DEBUG
	_debug( "con NOT found %lu\n", id );
#endif
	GLOBAL_UNLOCK();
	return NULL;
retl:
	GLOBAL_UNLOCK();
	cl->this = sv;
	return cl;
retf:
	GLOBAL_UNLOCK();
	cf->this = sv;
	return cf;
}

INLINE int dbe_con_reconnect( SV *obj, const char *obj_id, dbe_con_t *con ) {
	drv_def_t *dd = con->drv->def;
	unsigned int i;
	if( con->reconnect_count ) {
		if( dd->con_reconnect == NULL )
			NOFUNCTION_C( obj, obj_id, con, "reconnect" );
		for( i = 1; i <= con->reconnect_count; i ++ ) {
			if( i > 1 )
				sleep( 1 );
#ifdef DBE_DEBUG
			_debug( "reconnect %u / %u\n", i, con->reconnect_count );
#endif
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"-> reconnect (try %u of %u)", i, con->reconnect_count );
			if( dd->con_reconnect( con->drv_data ) == DBE_OK )
				goto success;
		}
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE, "<- reconnect= FALSE" );
	con->last_errno = DBE_CONN_LOST;
	return DBE_CONN_LOST;
success:
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE, "<- reconnect= TRUE" );
	return DBE_OK;
}

/*****************************************************************************
 * init / cleanup
 *****************************************************************************/

INLINE void dbe_free() {
	dbe_con_t *c1, *c2;
	dbe_drv_t *d1, *d2;
	if( global.destroyed )
		return;
	global.destroyed = 1;
	GLOBAL_LOCK();
	c1 = global.first_con;
	while( c1 != NULL ) {
		c2 = c1->next;
		dbe_con_free( c1 );
		c1 = c2;
	}
	global.first_con = global.last_con = NULL;
	d1 = global.first_drv;
	while( d1 != NULL ) {
		d2 = d1->next;
		Safefree( d1->name );
		Safefree( d1->class_con );
		Safefree( d1->class_res );
		Safefree( d1->class_stmt );
		if( d1->def->drv_free != NULL )
			d1->def->drv_free( d1->def );
		Safefree( d1 );
		d1 = d2;
	}
	global.first_drv = global.last_drv = NULL;
	GLOBAL_UNLOCK();
	SvREFCNT_dec( global.sv_trace_out );
#if DBE_DEBUG > 1
	debug_free();
#endif
}

INLINE void dbe_cleanup() {
	dbe_con_t *c1;
	GLOBAL_LOCK();
	for( c1 = global.first_con; c1 != NULL; c1 = c1->next )
		dbe_con_cleanup( c1 );
	global.cleanup = 0;
	GLOBAL_UNLOCK();
}

INLINE dbe_drv_t *dbe_init_driver( const char *name, size_t name_len ) {
	char *key;
	drv_def_t *dd;
	dbe_drv_t *drv;
	GLOBAL_LOCK();
	for( drv = global.last_drv; drv != NULL; drv = drv->prev ) {
		if( my_stricmp( drv->name, name ) == 0 ) {
			goto _exit;
		}
	}
	Newxz( drv, 1, dbe_drv_t );
	Newx( drv->name, name_len + 1, char );
	my_strcpyu( drv->name, name );
	{
		/* call perl function init_driver() in driver module */
		dSP;
		int count;
		char *tmp;
		Newx( tmp, name_len + 20, char );
		key = my_strcpy( tmp, "DBE::Driver::" );
		key = my_strcpyu( key, name );
		key = my_strcpy( key, "::init" );
		IF_TRACE_GLOBAL( DBE_TRACE_ALL )
			TRACE( NULL, DBE_TRACE_ALL, FALSE,
				"   init driver %s -> &%s", name, tmp );
		ENTER;
		SAVETMPS;
		PUSHMARK( SP );
		XPUSHs( sv_2mortal( newSViv( PTR2IV( &g_dbe ) ) ) );
		XPUSHs( sv_2mortal( newSViv( PTR2IV( drv ) ) ) );
		PUTBACK;
		count = call_pv( tmp, G_SCALAR | G_EVAL );
		SPAGAIN;
		if( count > 0 )
			drv->def = INT2PTR( drv_def_t *, POPi );
		PUTBACK;
		FREETMPS;
		LEAVE;
		Safefree( tmp );
	}
	if( SvTRUE( ERRSV ) ) {
		dbe_drv_set_error( NULL, -1, SvPV_nolen( ERRSV ) );
		drv->def = NULL;
	}
	dd = drv->def;
	if( dd == NULL ) {
		Safefree( drv->name );
		Safefree( drv );
		drv = NULL;
	}
	else {
		if( drv->class_con == NULL ) {
			Newx( drv->class_con, 9, char );
			Copy( "DBE::CON", drv->class_con, 9, char );
		}
		if( drv->class_res == NULL ) {
			Newx( drv->class_res, 9, char );
			Copy( "DBE::RES", drv->class_res, 9, char );
		}
		if( drv->class_stmt == NULL ) {
			Newx( drv->class_stmt, 10, char );
			Copy( "DBE::STMT", drv->class_stmt, 10, char );
		}
#ifdef DBE_DEBUG
		_debug( "driver init [%s]\n", dd->description );
#else
#endif
		IF_TRACE_GLOBAL( DBE_TRACE_ALL )
			TRACE( NULL, DBE_TRACE_ALL, FALSE,
				"   driver init [%s]", dd->description );
		if( global.first_drv == NULL )
			global.first_drv = drv;
		else {
			global.last_drv->next = drv;
			drv->prev = global.last_drv;
		}
		global.last_drv = drv;
	}
_exit:
	GLOBAL_UNLOCK();
	return drv;
}

/*****************************************************************************
 * virtual result set
 *****************************************************************************/

INLINE SV *dbe_vrt_res_fetch( int type, const char *data, size_t data_len ) {
	switch( type ) {
	case DBE_TYPE_VARCHAR:
	case DBE_TYPE_BINARY:
	default:
		if( data != NULL )
			return newSVpvn( data, data_len );
		break;
	case DBE_TYPE_INTEGER:
		if( data != NULL )
			return newSViv( *((int *) data) );
		break;
	case DBE_TYPE_DOUBLE:
		if( data != NULL )
			return newSVnv( *((double *) data) );
		break;
	case DBE_TYPE_BIGINT:
		if( data != NULL ) {
			xlong v = *((xlong *) data);
#if IVSIZE >= 8
			return newSViv( v );
#else
			if( v >= -2147483647 && v <= 2147483647 ) {
				return newSViv( (IV) v );
			}
			else {
				char tmp[32], *p1;
				p1 = my_ltoa( tmp, v, 10 );
				return newSVpvn( tmp, p1 - tmp );
			}
#endif
		}
		break;
	}
	return newSV(0);
}

INLINE int dbe_vrt_res_fetch_hash( dbe_res_t *res, HV **phv, int nc ) {
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	u_long i;
	char *name, *p1;
	STRLEN name_len;
	HV *hv;
	SV *sv;
	vres = (dbe_vrt_res_t *) res->drv_data;
	if( vres->row_pos >= vres->row_count )
		return DBE_NO_DATA;
	vrow = vres->rows + vres->row_pos ++;
	if( (*phv) == NULL )
		(*phv) = hv = (HV *) sv_2mortal( (SV *) newHV() );
	else
		hv = (*phv);
	switch( nc ) {
	case 1:
		name = NULL;
		name_len = 0;
		for( i = 0; i < vres->column_count; i ++ ) {
			if( name_len < vres->columns[i].name_length ) {
				name_len = vres->columns[i].name_length;
				Renew( name, name_len + 1, char );
			}
			p1 = my_strcpyl( name, vres->columns[i].name );
			sv = dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store( hv, name, (I32) (p1 - name), sv, 0 );
		}
		Safefree( name );
		break;
	case 2:
		name = NULL;
		name_len = 0;
		for( i = 0; i < vres->column_count; i ++ ) {
			if( name_len < vres->columns[i].name_length ) {
				name_len = vres->columns[i].name_length;
				Renew( name, name_len + 1, char );
			}
			p1 = my_strcpyu( name, vres->columns[i].name );
			sv = dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store( hv, name, (I32) (p1 - name), sv, 0 );
		}
		Safefree( name );
		break;
	default:
		for( i = 0; i < vres->column_count; i ++ ) {
			sv = dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			(void) hv_store( hv,
				vres->columns[i].name_conv,
				(I32) vres->columns[i].name_conv_length,
				sv, 0
			);
		}
		break;
	}
	return DBE_OK;
}

INLINE int dbe_vrt_res_fetchall_arrayref( dbe_res_t *res, AV **pav, int fn ) {
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow, *vrow_end;
	u_long i;
	AV *av, *av2;
	HV *hv;
	vres = (dbe_vrt_res_t *) res->drv_data;
	(*pav) = av = (AV *) sv_2mortal( (SV *) newAV() );
	vrow = vres->rows + vres->row_pos;
	vrow_end = vres->rows + vres->row_count;
	if( fn ) {
		for( ; vrow < vrow_end; vrow ++ ) {
			hv = (HV *) sv_2mortal( (SV *) newHV() );
			for( i = 0; i < vres->column_count; i ++ ) {
				(void) hv_store( hv,
					vres->columns[i].name_conv,
					(I32) vres->columns[i].name_conv_length,
					dbe_vrt_res_fetch(
						vres->columns[i].type, vrow->data[i],
						vrow->lengths[i]
					), 0
				);
			}
			av_push( av, newRV( (SV *) hv ) );
		}
	}
	else {
		for( ; vrow < vrow_end; vrow ++ ) {
			av2 = (AV *) sv_2mortal( (SV *) newAV() );
			for( i = 0; i < vres->column_count; i ++ ) {
				av_push( av2,
					dbe_vrt_res_fetch(
						vres->columns[i].type, vrow->data[i],
						vrow->lengths[i]
					)
				);
			}
			av_push( av, newRV( (SV *) av2 ) );
		}
	}
	vres->row_pos = vres->row_count;
	return DBE_OK;
}

INLINE int dbe_vrt_res_fetchall_hashref(
	dbe_res_t *res, dbe_str_t *fields, u_long ks, HV **phv
) {
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	u_long i, j;
	HV *root, *hv, *hv2;
	SV **psv;
	char *skey;
	STRLEN lkey;
	int *index;
	Newx( index, ks, int );
	vres = (dbe_vrt_res_t *) res->drv_data;
	for( i = 0; i < ks; i ++ ) {
		index[i] = -1;
		for( j = 0; j < vres->column_count; j ++ ) {
			if( strcmp( fields[i].value, vres->columns[j].name_conv ) == 0 ) {
				index[i] = (int) j;
				break;
			}
		}
		if( index[i] < 0 ) {
			dbe_drv_set_error( res->con, -1,
				"Field '%s' does not exist", fields[i].value );
			goto _error;
		}
	}
	(*phv) = root = (HV *) sv_2mortal( (SV *) newHV() );
	for( j = vres->row_pos; j < vres->row_count; j ++ ) {
		vrow = vres->rows + j;
		for( hv = root, i = 0; i < ks; i ++ ) {
			skey = vrow->data[index[i]];
			lkey = vrow->lengths[index[i]];
			psv = hv_fetch( hv, skey, (I32) lkey, 0 );
			if( psv != NULL )
				hv = (HV *) SvRV( (*psv) );
			else {
				hv2 = (HV *) sv_2mortal( (SV *) newHV() );
				(void) hv_store( hv,
					skey, (I32) lkey, newRV( (SV *) hv2 ), 0 );
				hv = hv2;
			}
		}
		for( i = 0; i < vres->column_count; i ++ ) {
			(void) hv_store( hv,
				vres->columns[i].name_conv,
				(I32) vres->columns[i].name_conv_length,
				dbe_vrt_res_fetch(
					vres->columns[i].type, vrow->data[i],
					vrow->lengths[i]
				), 0
			);
		}
	}
	vres->row_pos = vres->row_count;
	Safefree( index );
	return DBE_OK;
_error:
	Safefree( index );
	return DBE_ERROR;
}

/*****************************************************************************
 * lob stream functions
 *****************************************************************************/

INLINE void *dbe_lob_stream_find( SV *sv, dbe_con_t **pcon ) {
	dbe_con_t *con;
	dbe_lob_stream_t *ls;
	u_long id;
	if( global.destroyed )
		return NULL;
	if( ! SvROK( sv ) || ! (sv = SvRV( sv )) || ! SvIOK( sv ) )
		return NULL;
	id = (u_long) SvIV( sv );
	GLOBAL_LOCK();
	for( con = global.last_con; con != NULL; con = con->prev ) {
		for( ls = con->last_lob; ls != NULL; ls = ls->prev ) {
			if( ls->id == id )
				goto _found;
		}
	}
#ifdef DBE_DEBUG
	_debug( "lob stream %lu not found\n", id );
#endif
	GLOBAL_UNLOCK();
	return NULL;
_found:
	GLOBAL_UNLOCK();
	if( pcon != NULL )
		*pcon = con;
	return ls;
}

INLINE void dbe_lob_stream_rem( dbe_con_t *con, dbe_lob_stream_t *ls ) {
	CON_LOCK( con );
	if( ls->prev != NULL )
		ls->prev->next = ls->next;
	else if( ls == con->first_lob )
		con->first_lob = ls->next;
	if( ls->next != NULL )
		ls->next->prev = ls->prev;
	else if( ls == con->last_lob )
		con->last_lob = ls->prev;
	CON_UNLOCK( con );
	dbe_lob_stream_free( con, ls );
}

INLINE void dbe_lob_stream_free( dbe_con_t *con, dbe_lob_stream_t *ls ) {
	drv_def_t *dd;
	if( ! ls->closed ) {
		dd = con->drv->def;
		if( dd->con_lob_close != NULL )
			dd->con_lob_close( con->drv_data, ls->context );
	}
	Safefree( ls );
}

/*****************************************************************************
 * dbe misc functions
 *****************************************************************************/

INLINE int dbe_dsn_parse( char *dsn, char ***pargs ) {
	char *p1, *p2, *p3, **args = NULL;
	int ec = 0, is = 0, argc = 0, step = 0;
	for( p1 = dsn, p2 = dsn; *p1 != '\0'; p1 ++ ) {
		switch( *p1 ) {
		case '=':
			if( ! ec && ! is ) {
				for( ; p2 < p1 && (isSPACE( *p2 ) || *p2 == '\0'); p2 ++ );
				for( p3 = p1; p3 > p2 && isSPACE( *(p3 - 1) ); p3 -- );
				*p3 = '\0';
				if( p3 <= p2 )
					continue;
#ifdef DBE_DEBUG
				_debug( "DSN parser found key %d [%s]\n", p3 - p2, p2 );
#endif
				if( (argc % 8) == 0 )
					Renew( args, argc + 8, char * );
				args[argc ++] = p2;
				p2 = p1 + 1;
				step = 1;
			}
			break;
		case ';':
			if( ! ec && ! is ) {
				for( ; p2 < p1 && (isSPACE( *p2 ) || *p2 == '\0'); p2 ++ );
				for( p3 = p1; p3 > p2 && isSPACE( *(p3 - 1) ); p3 -- );
				*p3 = '\0';
				if( p3 <= p2 )
					continue;
				if( (argc % 8) == 0 )
					Renew( args, argc + 8, char * );
				args[argc ++] = p2;
				if( step == 1 ) {
#ifdef DBE_DEBUG
					_debug( "DSN parser found val %d [%s]\n", p3 - p2, p2 );
#endif
					p2 = p1 + 1;
					step = 0;
				}
				else {
#ifdef DBE_DEBUG
					_debug( "DSN parser found key %d [%s]\n", p3 - p2, p2 );
#endif
					if( (argc % 8) == 0 )
						Renew( args, argc + 8, char * );
					args[argc ++] = "";
					p2 = p1 + 1;
					step = 1;
				}
			}
			break;
		case '"':
			if( ! ec ) {
				if( ! is ) {
					is = 1;
					p2 ++;
				}
				else if( is == 1 ) {
					is = 0;
					*p1 = '\0';
				}
			}
			break;
		case '\'':
			if( ! ec ) {
				if( ! is ) {
					is = 2;
					p2 ++;
				}
				else if( is == 2 ) {
					is = 0;
					*p1 = '\0';
				}
			}
			break;
		case '\\':
			ec = ! ec;
			continue;
		}
		ec = 0;
	}
	if( p2 < p1 ) {
		for( ; p2 < p1 && (isSPACE( *p2 ) || *p2 == '\0'); p2 ++ );
		for( ; p1 > p2 && isSPACE( *(p1 - 1) ); p1 -- );
#ifdef DBE_DEBUG
		_debug( "DSN parser found %s %d [%s]\n", step ? "val" : "key", p1 - p2, p2 );
#endif
		if( (argc % 8) == 0 )
			Renew( args, argc + 8, char * );
		args[argc ++] = p2;
		if( step == 0 ) {
			if( (argc % 8) == 0 )
				Renew( args, argc + 8, char * );
			args[argc ++] = "";
		}
	}
	if( argc % 2 )
		args[argc ++] = "";
	(*pargs) = args;
	return argc;
}

INLINE SV *dbe_quote( const char *str, size_t str_len ) {
	const char *sp = str, *se = str + str_len;
	char *tmp, *so;
	SV *sv;
	unsigned int wc;
	Newx( tmp, str_len * 2 + 3, char );
	so = tmp;
	*so ++ = '\'';
	for( ; sp < se; sp ++ ) {
		wc = (unsigned char) *sp;
		/* skip utf-8 characters */
		if( wc & 0x80 ) {
			*so ++ = (char) wc;
			if( (sp[1] & 0xC0) != 0x80 )
				continue;
			wc &= 0x3F;
			if( wc & 0x20 ) {
				if( (sp[2] & 0xC0) != 0x80 )
					continue;
				if( ++ sp == se )
					break;
				*so ++ = *sp;
			}
			if( ++ sp == se )
				break;
			*so ++ = *sp;
		}
		/* escape quote character */
		else if( wc == '\'' ) {
			*so ++ = '\'';
			*so ++ = '\'';
		}
		else {
			*so ++ = (char) wc;
		}
	}
	*so ++ = '\'';
	sv = newSVpvn( tmp, so - tmp );
	Safefree( tmp );
	return sv;
}

INLINE SV *dbe_quote_id(
	const char **args, int argc,
	const char quote_id_prefix, const char quote_id_suffix,
	const char catalog_sep, const short catalog_location
) {
	SV *sv;
	char *tmp, *so, *se, cc;
	const char *sp;
	size_t len = 138 + argc * 32;
	int i;
	unsigned int wc;
	Newx( tmp, len, char );
	so = tmp;
	se = tmp + len - 10;
	if( argc >= 3 ) {
		if( catalog_location == SQL_CL_END ) {
			i = 1;
			cc = '.';
		}
		else {
			i = 0;
			cc = catalog_sep;
		}
	}
	else {
		i = 0;
		cc = '.';
	}
	for( ; i < argc; i ++ ) {
quote_item:
		sp = args[i];
		if( *sp == '\0' )
			continue;
		*so ++ = quote_id_prefix;
		while( 1 ) {
			wc = *sp ++;
			if( so >= se ) {
				Renew( tmp, len + 138, char );
				so = tmp + len;
				se = so + 128;
			}
			/* skip utf-8 characters */
			if( wc & 0x80 ) {
				*so ++ = (char) wc;
				if( (sp[1] & 0xC0) != 0x80 )
					continue;
				wc &= 0x3F;
				if( wc & 0x20 ) {
					if( (sp[2] & 0xC0) != 0x80 )
						continue;
					*so ++ = *sp ++;
				}
				if( *sp == '\0' )
					break;
				*so ++ = *sp ++;
			}
			/* stop at the end */
			else if( wc == '\0' ) {
				*so ++ = quote_id_suffix;
				break;
			}
			/* escape quote character */
			else if( wc == quote_id_prefix ) {
				*so ++ = quote_id_prefix;
				*so ++ = quote_id_prefix;
			}
			else if( wc == quote_id_suffix ) {
				*so ++ = quote_id_suffix;
				*so ++ = quote_id_suffix;
			}
			else {
				*so ++ = (char) wc;
			}
		}
		*so ++ = cc;
		cc = '.';
	}
	so --;
	if( argc >= 3 && catalog_location == SQL_CL_END ) {
		*so ++ = catalog_sep;
		argc = i = 0;
		goto quote_item;
	}
	if( so > tmp )
		sv = newSVpvn( tmp, so - tmp );
	else
		sv = newSV( 0 );
	Safefree( tmp );
	return sv;
}

INLINE SV *dbe_escape_pattern(
	const char *str, size_t str_len, const char esc
) {
	const char *sp = str, *se = str + str_len;
	char *tmp, *so;
	SV *sv;
	unsigned int wc;
	Newx( tmp, str_len * 2 + 3, char );
	so = tmp;
	for( ; sp < se; sp ++ ) {
		wc = (unsigned char) *sp;
		/* skip utf-8 characters */
		if( wc & 0x80 ) {
			*so ++ = (char) wc;
			if( (sp[1] & 0xC0) != 0x80 )
				continue;
			wc &= 0x3F;
			if( wc & 0x20 ) {
				if( (sp[2] & 0xC0) != 0x80 )
					continue;
				if( ++ sp == se )
					break;
				*so ++ = *sp;
			}
			if( ++ sp == se )
				break;
			*so ++ = *sp;
			continue;
		}
		/* escape pattern identifiers */
		switch( wc ) {
		case '_':
		case '%':
		case '[':
		case ']':
		case '^':
		case '$':
		case '!':
		case '*':
			*so ++ = esc;
			break;
		}
		*so ++ = (char) wc;
	}
	sv = newSVpvn( tmp, so - tmp );
	Safefree( tmp );
	return sv;
}

INLINE char *dbe_convert_query(
	dbe_con_t *con, const char *query, size_t query_len, size_t *res_len,
	char ***p_params, int *p_param_count
) {
	char *res, *s1, qip, qis, csc, *ids = NULL, *ide = NULL, **params = NULL;
	const char *cts = NULL, *query_end;
	int esc = 0, iss = 0, ppos = 0;
	size_t len;
	if( query_len == 0 )
		query_len = strlen( query );
	Newx( res, query_len * 2 + 1, char );
	while( isSPACE( *query ) ) {
		query ++;
		query_len --;
	}
	qip = con->quote_id_prefix;
	qis = con->quote_id_suffix;
	csc = con->catalog_separator;
	s1 = res;
	for( query_end = query + query_len; query < query_end; query ++ ) {
/*
#ifdef DBE_DEBUG
		_debug( "%c, esc %d, iss %d\n", *query, esc, iss );
#endif
*/
		switch( *query ) {
		case '\\':
			esc = ! esc;
			*s1 ++ = '\\';
			continue;
		case '\'':
			if( ! esc ) {
				if( iss == 1 )
					iss = 0;
				else
				if( ! iss )
					iss = 1;
			}
			*s1 ++ = '\'';
			break;
		case '"':
			if( ! esc ) {
				if( iss == 2 )
					iss = 0;
				else
				if( ! iss )
					iss = 2;
			}
			*s1 ++ = '"';
			break;
		case '[':
			if( ! esc && ! iss ) {
				ids = s1;
				if( query[1] == '*' ) {
					iss = 4;
				}
				else {
					iss = 3;
					*s1 ++ = qip;
				}
			}
			else
				*s1 ++ = '[';
			break;
		case ']':
			if( esc || (iss >= 0 && iss <= 2) )
				*s1 ++ = ']';
			else {
				if( iss == 3 )
					*s1 ++ = qis;
				else
				if( iss == 5 ) {
					if( con->catalog_location == SQL_CL_START ) {
						size_t ctl = query - cts;
						/* table@catalog = "table"."catalog */
						Move( ids, ids + ctl + 3, ide - ids, char );
						Copy( cts, ids + 1, ctl, char );
						*ids = qip;
						ids += ctl + 1;
						*ids ++ = qis;
						*ids = csc;
					}
					*s1 ++ = qis;
				}
				iss = 0;
			}
			break;
		case '#':
			if( ! esc && iss >= 3 )
				*s1 ++ = '.';
			else
				*s1 ++ = '#';
			break;
		case '.':
			if( ! esc && iss >= 3 ) {
				if( iss == 3 )
					*s1 ++ = qis;
				*s1 ++ = '.';
				if( query[1] == '*' )
					iss = 4;
				else {
					*s1 ++ = qip;
					iss = 3;
				}
			}
			else
				*s1 ++ = '.';
			break;
		case '@':
			if( ! esc && iss >= 3 ) {
				if( iss == 3 )
					*s1 ++ = qis;
				ide = s1;
				*s1 ++ = csc;
				*s1 ++ = qip;
				cts = query + 1;
				iss = 5;
			}
			else
				*s1 ++ = '@';
			break;
		case '?':
			if( ! esc && ! iss ) {
				if( p_params != NULL && (ppos % 8) == 0 ) {
					Renew( params, ppos + 8, char * );
					Zero( params + ppos, 8, char * );
				}
				ppos ++;
			}
			*s1 ++ = '?';
			break;
		case ':':
			if( ! esc && ! iss ) {
				*s1 ++ = '?';
				for( query ++, cts = query; ; query ++ ) {
					if( ! isALNUM( *query ) || query == query_end ) {
						if( p_params != NULL ) {
							len = query - cts;
							if( (ppos % 8) == 0 ) {
								Renew( params, ppos + 8, char * );
								Zero( params + ppos, 8, char * );
							}
							Newx( params[ppos], len + 1, char );
							Copy( cts, params[ppos], len, char );
							params[ppos][len] = '\0';
#ifdef DBE_DEBUG
							_debug( "found named parameter (%d) %lu %s\n",
								ppos, len, params[ppos] );
#endif
						}
						ppos ++;
						query --;
						break;
					}
				}
				continue;
			}
			else
				*s1 ++ = ':';
			break;
		default:
			if( iss == 3 ) {
				if( *query == qip )
					*s1 ++ = qip, *s1 ++ = qip;
				else if( *query == qis )
					*s1 ++ = qis, *s1 ++ = qis;
				else
					*s1 ++ = *query;
			}
			else
				*s1 ++ = *query;
		}
		esc = 0;
	}
	*s1 = '\0';
	/*
#ifdef DBE_DEBUG
	_debug( "SQL: [%s]\n", res );
#endif
	*/
	if( p_params != NULL )
		(*p_params) = params;
	if( p_param_count != NULL )
		(*p_param_count) = ppos;
	(*res_len) = s1 - res;
	return res;
}

INLINE enum sql_type dbe_sql_type_by_name( const char *name ) {
	if( my_strnicmp( name, "SQL_", 4 ) == 0 )
		name += 4;
	switch( toupper( *name ) ) {
	case 'A':
		if( my_stricmp( name, "ALL" ) == 0 )
			return SQL_ALL_TYPES;
		if( my_stricmp( name, "ALL_TYPES" ) == 0 )
			return SQL_ALL_TYPES;
		break;
	case 'B':
		if( my_stricmp( name, "BIGINT" ) == 0 )
			return SQL_BIGINT;
		if( my_stricmp( name, "BINARY" ) == 0 )
			return SQL_BINARY;
		if( my_stricmp( name, "BOOLEAN" ) )
			return SQL_BOOLEAN;
		break;
	case 'C':
		if( my_stricmp( name, "CHAR" ) == 0 )
			return SQL_CHAR;
		if( my_stricmp( name, "CLOB" ) == 0 )
			return SQL_CLOB;
		break;
	case 'D':
		if( my_stricmp( name, "DATETIME" ) == 0 )
			return SQL_DATETIME;
		if( my_stricmp( name, "DATE" ) == 0 )
			return SQL_DATE;
		if( my_stricmp( name, "DOUBLE" ) == 0 )
			return SQL_DOUBLE;
		if( my_stricmp( name, "DECIMAL" ) == 0 )
			return SQL_DECIMAL;
		break;
	case 'F':
		if( my_stricmp( name, "FLOAT" ) == 0 )
			return SQL_FLOAT;
		break;
	case 'I':
		if( my_stricmp( name, "INT" ) == 0 ||
			my_stricmp( name, "INTEGER" ) == 0 )
			return SQL_INTEGER;
		break;
	case 'N':
		if( my_stricmp( name, "NUMERIC" ) )
			return SQL_NUMERIC;
		break;
	case 'R':
		if( my_stricmp( name, "REAL" ) == 0 )
			return SQL_REAL;
		break;
	case 'S':
		if( my_stricmp( name, "SMALLINT" ) == 0 )
			return SQL_SMALLINT;
		break;
	case 'T':
		if( my_stricmp( name, "TIME" ) == 0 )
			return SQL_TIME;
		if( my_stricmp( name, "TINYINT" ) == 0 )
			return SQL_TINYINT;
		break;
	case 'V':
		if( my_stricmp( name, "VARCHAR" ) == 0 )
			return SQL_VARCHAR;
		if( my_stricmp( name, "VARBINARY" ) == 0 )
			return SQL_VARBINARY;
		break;
	}
	return SQL_UNKNOWN_TYPE;
}

INLINE int dbe_trace_level_by_name( const char *name ) {
	if( name == NULL )
		return DBE_TRACE_NONE;
	if( my_strnicmp( name, "TRACE_", 6 ) == 0 )
		name += 6;
	if( my_stricmp( name, "SQL" ) == 0 )
		return DBE_TRACE_SQL;
	if( my_stricmp( name, "SQLFULL" ) == 0 )
		return DBE_TRACE_SQLFULL;
	return DBE_TRACE_ALL;
}

INLINE const char *
dbe_trace_name_by_level( int level ) {
	if( level <= DBE_TRACE_NONE )
		return NULL;
	switch( level ) {
	case DBE_TRACE_SQL:
		return "SQL";
	case DBE_TRACE_SQLFULL:
		return "SQLFULL";
	}
	return "ALL";
}

INLINE int dbe_bind_param_num(
	SV *obj, const char *obj_id, dbe_con_t *con, drv_stmt_t *stmt,
	drv_def_t *dd, int p_num, SV *p_val, char p_type, const char *p_name
) {
	int r;
	if( dd->stmt_bind_param == NULL ) {
		dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "bind_param" );
		return DBE_ERROR;
	}
	r = dd->stmt_bind_param( stmt, p_num, p_val, p_type );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL ) {
			TRACE( con, DBE_TRACE_SQLFULL, TRUE,
				"-> bind_param(num: %d, name: '%s', value: '%s')= TRUE",
				p_num, p_name ? p_name : "", SvPV_nolen( p_val )
			);
		}
		return DBE_OK;
	case DBE_CONN_LOST:
		return DBE_CONN_LOST;
	default:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL ) {
			TRACE( con, DBE_TRACE_SQLFULL, TRUE,
				"-> bind_param(num: %d, name: '%s', value: '%s')= FALSE",
				p_num, p_name ? p_name : "", SvPV_nolen( p_val )
			);
		}
	}
	return DBE_ERROR;
}

INLINE int dbe_bind_param_str(
	SV *obj, const char *obj_id, dbe_con_t *con, drv_stmt_t *stmt,
	drv_def_t *dd, char **params, int param_count,
	char *p_num, SV *p_val, char p_type
) {
	int r, i;
	if( isDIGIT( *p_num ) ) {
		r = dbe_bind_param_num(
			obj, obj_id, con, stmt, dd, atoi( p_num ), p_val, p_type, p_num );
		if( r != DBE_OK )
			return r;
	}
	else {
		if( *p_num == ':' )
			p_num ++;
		r = DBE_ERROR;
		if( params != NULL ) {
			for( i = 0; i < param_count; i ++ ) {
				if( params[i] != NULL &&
					my_stricmp( params[i], p_num ) == 0
				) {
					r = dbe_bind_param_num(
						obj, obj_id, con, stmt, dd, i + 1, p_val, p_type, p_num
					);
					if( r != DBE_OK )
						return r;
				}
			}
		}
		if( r != DBE_OK ) {
			dbe_drv_set_error( con,
				-1, "Invalid parameter name \"%s\"", p_num );
			return DBE_ERROR;
		}
	}
	return DBE_OK;
}

void dbe_handle_error(
	SV *obj, const char *obj_id, dbe_con_t *con, const char *action
) {
	if( con->error_handler != NULL ) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		if( con->error_handler_arg != NULL ) {
			XPUSHs( con->error_handler_arg );
		}
		XPUSHs( sv_2mortal( newSViv( con->last_errno ) ) );
		XPUSHs( sv_2mortal( newSVpv( con->last_error, 0 ) ) );
		XPUSHs( sv_2mortal( newSVpv( con->drv->def->description, 0 ) ) );
		XPUSHs( sv_2mortal( newSVpv( action, 0 ) ) );
		XPUSHs( sv_2mortal( newSVpv( obj_id, 0 ) ) );
		XPUSHs( obj );
		PUTBACK;
		if( con->error_handler_arg != NULL &&
			sv_isobject( con->error_handler_arg ) &&
			SvPOK( con->error_handler )
		) {
			call_method( SvPV_nolen( con->error_handler ), G_DISCARD );
		}
		else {
			call_sv( con->error_handler, G_DISCARD );
		}
		FREETMPS;
		LEAVE;
	}
	else {
		if( con->flags & CON_ONERRORDIE )
			Perl_croak( aTHX_ "[%s] %s: %s",
				con->drv->def->description, action, con->last_error );
		if( con->flags & CON_ONERRORWARN )
			Perl_warn( aTHX_ "[%s] %s: %s",
				con->drv->def->description, action, con->last_error );
	}
}

INLINE void dbe_perl_printf( SV *sv, const char *msg, ... ) {
	va_list va;
	char *tmp;
	size_t len = strlen( msg ) + 4096;
	Newx( tmp, len, char );
	va_start( va, msg );
	len = vsnprintf( tmp, len, msg, va );
	va_end( va );
	dbe_perl_print( sv, tmp, len );
	Safefree( tmp );
}

INLINE void dbe_perl_print( SV *sv, const char *msg, size_t msg_len ) {
	IO *io = sv_2io( sv );
	const MAGIC *mg = SvTIED_mg( (SV *) io, 'q' );
	if( mg != NULL ) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs( SvTIED_obj( (SV *) io, mg ) );
		XPUSHs( sv_2mortal( newSVpvn( msg, msg_len ) ) );
		PUTBACK;
		call_method( "PRINT", G_DISCARD );
		FREETMPS;
		LEAVE;
	}
	else {
		PerlIO_printf( IoIFP( io ), msg );
	}
}

#define hv_copy(src,dst) \
	do { \
		register SV *__val; \
		char *__key; \
		I32 __klen; \
		hv_clear( (dst) ); \
		hv_iterinit( (src) ); \
		while( (__val = hv_iternextsv( (src), &__key, &__klen )) != NULL ) { \
			(void) hv_store( (dst), __key, __klen, newSVsv( __val ), 0 ); \
		} \
	} while( 0 )

#define DUMP_VAR_STRCPY(str) \
	do { \
		register const char *__p; \
		for( __p = (char *) (str); *__p != '\0'; out[out_pos ++] = *__p ++ ); \
	} while( 0 )

#define DUMP_VAR_INDENT(lvl) { \
	if( ! print ) \
		for( j = out_pos + (lvl); out_pos < j; out[out_pos ++] = '\t' ); \
	else \
		for( j = out_pos + (lvl) * 4; out_pos < j; out[out_pos ++] = ' ' ); \
}

#define DUMP_VAR_GROW_SIZE		512

#define DUMP_VAR_ENSURE_OUT(s) \
	if( out_pos + (s) >= out_size ) { \
		out_size += (s) + DUMP_VAR_GROW_SIZE; \
		Renew( out, out_size, char ); \
	}
	
void my_dump_var_int(
	SV *sv, char **pout, size_t *pout_pos, size_t *pout_size, const char *name,
	HV *cache, HV *tree, size_t level, int print, char **plink, size_t linkpos,
	size_t linksize
) {
	size_t out_pos = (*pout_pos), out_size = (*pout_size), linkps;
	char *out = (*pout), *link = (*plink);
	HV *parent_tree = (HV *) sv_2mortal( (SV *) newHV() );
	hv_copy( tree, parent_tree );
	if( sv == NULL )
		goto dump_null;
	/*
#if DBE_DEBUG
	_debug( "dump_var type %d\n", name, SvTYPE( sv ) );
#endif
	*/
	switch( SvTYPE( sv ) ) {
	case SVt_NULL:
dump_null:
		if( name != NULL ) {
			if( ! print ) {
				out[out_pos ++] = '$';
				for( ; *name != '\0'; out[out_pos ++] = *name ++ );
				DUMP_VAR_STRCPY( " = undef;\n" );
			}
			else {
				DUMP_VAR_STRCPY( "undef" );
			}
		}
		else {
			DUMP_VAR_ENSURE_OUT( 64 );
			DUMP_VAR_STRCPY( "undef" );
		}
		break;
	case SVt_PVIV:
		if( SvROK( sv ) )
			goto dump_rv;
		if( SvPOK( sv ) )
			goto dump_pv;
		if( SvNOK( sv ) )
			goto dump_nv;
	case SVt_IV:
dump_iv:
		if( name != NULL ) {
			if( ! print ) {
				out[out_pos ++] = '$';
				for( ; *name != '\0'; out[out_pos ++] = *name ++ );
				DUMP_VAR_STRCPY( " = " );
			}
		}
		else
			DUMP_VAR_ENSURE_OUT( 64 );
		out_pos = my_ltoa( out + out_pos, SvIV( sv ), 10 ) - out;
		if( name != NULL && ! print ) {
			out[out_pos ++] = ';';
			out[out_pos ++] = '\n';
		}
		break;
	case SVt_PVNV:
		if( SvROK( sv ) )
			goto dump_rv;
		if( SvPOK( sv ) )
			goto dump_pv;
		if( SvIOK( sv ) )
			goto dump_iv;
	case SVt_NV:
dump_nv:
		if( name != NULL ) {
			if( ! print ) {
				out[out_pos ++] = '$';
				for( ; *name != '\0'; out[out_pos ++] = *name ++ );
				DUMP_VAR_STRCPY( " = " );
			}
		}
		else
			DUMP_VAR_ENSURE_OUT( 64 );
		out_pos = my_ftoa( out + out_pos, SvNV( sv ) ) - out;
		if( name != NULL && ! print ) {
			out[out_pos ++] = ';';
			out[out_pos ++] = '\n';
		}
		break;
	case SVt_PV:
dump_pv:
		{
			STRLEN len;
			unsigned char *s2, *str, *end;
			char *s1;
			str = (unsigned char *) SvPVx( sv, len );
			end = str + len;
			if( name != NULL ) {
				if( ! print ) {
					out[out_pos ++] = '$';
					for( ; *name != '\0'; out[out_pos ++] = *name ++ );
					DUMP_VAR_STRCPY( " = " );
				}
			}
			DUMP_VAR_ENSURE_OUT( len * 6 + 64 );
			if( ! print ) {
				len = 0;
				for( s2 = str; s2 < end; s2 ++ ) {
					if( *s2 == '\\' || *str < 32 || *str > 127 ) {
						len = 1;
						break;
					}
				}
				s1 = out + out_pos;
				if( ! len ) {
					*s1 ++ = '\'';
					for( ; str < end; str ++ ) {
						if( *str == '\'' )
							*s1 ++ = '\\';
						*s1 ++ = *str;
					}
					*s1 ++ = '\'';
				}
				else {
					*s1 ++ = '"';
					for( ; str < end; str ++ ) {
						switch( *str ) {
						case '\\':
							*s1 ++ = '\\';
							*s1 ++ = '\\';
							continue;
						case '"': case '$': case '@': case '%':
							*s1 ++ = '\\';
							*s1 ++ = *str;
							break;
						default:
							if( *str < 32 || *str > 127 ) {
								*s1 ++ = '\\';
								*s1 ++ = 'x';
								*s1 ++ = '{';
								*s1 ++ = HEXTAB[*str / 16];
								*s1 ++ = HEXTAB[*str % 16];
								*s1 ++ = '}';
							}
							else
								*s1 ++ = *str;
						}
					}
					*s1 ++ = '"';
				}
				if( name != NULL ) {
					*s1 ++ = ';';
					*s1 ++ = '\n';
				}
				out_pos = s1 - out;
			}
			else {
				DUMP_VAR_STRCPY( str );
			}
		}
		break;
	case SVt_RV:
dump_rv:
		{
			STRLEN len;
			char *key = SvPV( sv, len ), *pkg;
			SV **psv;
			HV *stash;
			if( sv_isobject( sv ) ) {
				stash = SvSTASH( SvRV( sv ) );
				pkg = HvNAME( stash );
				/*printf( "blessed into %s\n", pkg );*/
			}
			else
				pkg = NULL;
			if( name != NULL ) {
				if( ! print ) {
					out[out_pos ++] = '$';
					for( ; *name != '\0'; out[out_pos ++] = *name ++ );
					DUMP_VAR_STRCPY( " = " );
					link[linkpos ++] = '-';
					link[linkpos ++] = '>';
				}
			}
			else if( (psv = hv_fetch( cache, key, (I32) len, 0 )) != NULL ) {
				/* known reference */
				if( print ) {
					if( hv_exists( tree, key, len ) ) {
						/* cross reference */
						DUMP_VAR_ENSURE_OUT( 64 );
						DUMP_VAR_STRCPY( "--stopped on cross reference" );
						goto finish;
					}
				}
				else {
					/* link to reference */
					key = SvPVx( *psv, len );
					DUMP_VAR_ENSURE_OUT( len + 64 );
					DUMP_VAR_STRCPY( key );
					goto finish;
				}
			}
			else {
				(void) hv_store(
					cache, key, (I32) len, newSVpvn( link, linkpos ), 0 );
			}
			(void) hv_store( tree, key, (I32) len, &PL_sv_yes, 0 );
			if( pkg != NULL ) {
				if( ! print ) {
					DUMP_VAR_STRCPY( "bless( " );
				}
				else {
					len = strlen( pkg );
					DUMP_VAR_ENSURE_OUT( len + 64 );
					out[out_pos ++] = '(';
					DUMP_VAR_STRCPY( pkg );
					out[out_pos ++] = ')';
					out[out_pos ++] = ' ';
				}
			}
			my_dump_var_int(
				SvRV( sv ), &out, &out_pos, &out_size, NULL, cache, tree,
				level, print, &link, linkpos, linksize
			);
			if( pkg != NULL ) {
				if( ! print ) {
					len = strlen( pkg );
					DUMP_VAR_ENSURE_OUT( len + 64 );
					DUMP_VAR_STRCPY( ", '" );
					DUMP_VAR_STRCPY( pkg );
					DUMP_VAR_STRCPY( "' )" );
				}
			}
			if( name != NULL && ! print ) {
				out[out_pos ++] = ';';
				out[out_pos ++] = '\n';
			}
		}
		break;
	case SVt_PVAV:
		{
			I32 alen, i;
			size_t j;
			SV **psv;
			alen = av_len( (AV *) sv ) + 1;
			if( name != NULL ) {
				if( ! print ) {
					out[out_pos ++] = '@';
					for( ; *name != '\0'; out[out_pos ++] = *name ++ );
					DUMP_VAR_STRCPY( " = (\n" );
				}
			}
			else {
				DUMP_VAR_ENSURE_OUT( level * 8 + 64 );
				if( ! print )
					out[out_pos ++] = '[';
				else {
					DUMP_VAR_STRCPY( "Array\n" );
					DUMP_VAR_INDENT( level );
					out[out_pos ++] = '(';
				}
				out[out_pos ++] = '\n';
			}
			link[linkpos ++] = '[';
			linkps = linkpos;
			for( i = 0; i < alen; i ++ ) {
				DUMP_VAR_ENSURE_OUT( level * 4 + 64 );
				DUMP_VAR_INDENT( level + 1 );
				if( print ) {
					out[out_pos ++] = '[';
					out_pos = my_itoa( out + out_pos, i, 10 ) - out;
					DUMP_VAR_STRCPY( "] => " );
				}
				else {
					linkpos = linkps;
					if( linkpos + 32 >= linksize ) {
						linksize = linkpos + 32;
						Renew( link, linksize, char );
					}
					linkpos = my_itoa( link + linkpos, i, 10 ) - link;
					link[linkpos ++] = ']';
				}
				psv = av_fetch( (AV *) sv, i, FALSE );
				my_dump_var_int(
					psv != NULL ? *psv : NULL,
					&out, &out_pos, &out_size, NULL, cache, tree, level + 1,
					print, &link, linkpos, linksize
				);
				if( ! print )
					out[out_pos ++] = ',';
				out[out_pos ++] = '\n';
			}
			if( name != NULL ) {
				if( ! print )
					DUMP_VAR_STRCPY( ");\n" );
			}
			else {
				DUMP_VAR_INDENT( level );
				out[out_pos ++] = print ? ')' : ']';
			}
		}
		break;
	case SVt_PVHV:
		{
			SV *val;
			char *key;
			I32 klen;
			size_t j;
			hv_iterinit( (HV *) sv );
			if( name != NULL ) {
				if( ! print ) {
					out[out_pos ++] = '%';
					for( ; *name != '\0'; out[out_pos ++] = *name ++ );
					DUMP_VAR_STRCPY( " = (\n" );
				}
			}
			else {
				DUMP_VAR_ENSURE_OUT( level * 8 + 64 );
				if( ! print )
					out[out_pos ++] = '{';
				else {
					DUMP_VAR_STRCPY( "Hash\n" );
					DUMP_VAR_INDENT( level );
					out[out_pos ++] = '(';
				}
				out[out_pos ++] = '\n';
			}
			link[linkpos ++] = '{';
			link[linkpos ++] = '\'';
			linkps = linkpos;
			while( (val = hv_iternextsv( (HV *) sv, &key, &klen )) != NULL ) {
				DUMP_VAR_ENSURE_OUT( level * 4 + klen + 64 );
				DUMP_VAR_INDENT( level + 1 );
				if( ! print ) {
					out[out_pos ++] = '\'';
					DUMP_VAR_STRCPY( key );
					DUMP_VAR_STRCPY( "' => " );
					linkpos = linkps;
					if( linkpos + klen + 32 >= linksize ) {
						linksize = linkpos + klen + 32;
						Renew( link, linksize, char );
					}
					linkpos = my_strcpy( link + linkpos, key ) - link;
					link[linkpos ++] = '\'';
					link[linkpos ++] = '}';
				}
				else {
					out[out_pos ++] = '[';
					DUMP_VAR_STRCPY( key );
					DUMP_VAR_STRCPY( "] => " );
				}
				my_dump_var_int(
					val, &out, &out_pos, &out_size, NULL, cache, tree,
					level + 1, print, &link, linkpos, linksize
				);
				if( ! print )
					out[out_pos ++] = ',';
				out[out_pos ++] = '\n';
			}
			if( name != NULL ) {
				if( ! print )
					DUMP_VAR_STRCPY( ");\n" );
			}
			else {
				DUMP_VAR_INDENT( level );
				out[out_pos ++] = print ? ')' : '}';
			}
		}
		break;
	case SVt_PVMG:
		if( SvROK( sv ) )
			goto dump_rv;
		if( SvIOK( sv ) )
			goto dump_iv;
		if( SvNOK( sv ) )
			goto dump_nv;
		if( SvPOK( sv ) )
			goto dump_pv;
		goto dv_default;
	case SVt_PVCV:
		if( print ) {
			DUMP_VAR_ENSURE_OUT( 128 );
			DUMP_VAR_STRCPY( "[CODE REFERENCE (0x" );
			out_pos = my_ultoa( out + out_pos, (size_t) sv, 16 ) - out;
			DUMP_VAR_STRCPY( ")]" );
		}
		else
			goto dv_default;
		break;
	case SVt_PVGV:
		if( print ) {
			DUMP_VAR_ENSURE_OUT( 128 );
			DUMP_VAR_STRCPY( "[GLOBAL HANDLE (0x" );
			out_pos = my_ultoa( out + out_pos, (size_t) sv, 16 ) - out;
			DUMP_VAR_STRCPY( ")]" );
		}	
		else
			goto dv_default;
		break;
	case SVt_PVIO:
		if( print ) {
			DUMP_VAR_ENSURE_OUT( 128 );
			DUMP_VAR_STRCPY( "[IO HANDLE (0x" );
			out_pos = my_ultoa( out + out_pos, (size_t) sv, 16 ) - out;
			DUMP_VAR_STRCPY( ")]" );
		}	
		else
			goto dv_default;
		break;
	default:
dv_default:
		DUMP_VAR_ENSURE_OUT( 128 );
		if( name != NULL && ! print ) {
			out[out_pos ++] = '$';
			for( ; *name != '\0'; out[out_pos ++] = *name ++ );
			DUMP_VAR_STRCPY( " = undef; # -incompatible type-\n" );
		}
		else {
			if( ! print )
				DUMP_VAR_STRCPY( "undef, # -incompatible type-" );
			else {
				DUMP_VAR_STRCPY( "--unkown type " );
				out_pos = my_itoa( out + out_pos, SvTYPE( sv ), 10 ) - out;
				out[out_pos ++] = '-';
				out[out_pos ++] = '-';
			}
		}
		break;
	}
finish:
	(*pout) = out;
	(*pout_pos) = out_pos;
	(*pout_size) = out_size;
	(*plink) = link;
	hv_copy( parent_tree, tree );
}

char *my_dump_var( SV *sv, const char *name, size_t *rlen, int print ) {
	size_t str_pos = 0, str_size, linkpos = 0, linksize;
	char *str, *link;
	const char *s2;
	STRLEN len;
	HV *cache, *tree;
	cache = (HV *) sv_2mortal( (SV *) newHV() );
	tree = (HV *) sv_2mortal( (SV *) newHV() );
	if( SvROK( sv ) ) {
		str = SvPVx( sv, len );
		(void) hv_store( cache, str, (I32) len, &PL_sv_yes, 0 );
		(void) hv_store( tree, str, (I32) len, &PL_sv_yes, 0 );
	}
	if( name == NULL )
		name = "var";
	linksize = str_size = strlen( name ) + DUMP_VAR_GROW_SIZE;
	Newx( str, str_size + 128, char );
	Newx( link, linksize + 64, char );
	link[linkpos ++] = '$';
	for( s2 = name; *s2 != '\0'; link[linkpos ++] = *s2 ++ );
	if( *name == '%' || *name == '@' || *name == '*' )
		my_dump_var_int(
			SvROK(sv) ? SvRV(sv) : sv,
				&str, &str_pos, &str_size, name + 1, cache, tree, 0, print,
				&link, linkpos, linksize
		);
	else if( *name == '$' )
		my_dump_var_int(
			sv, &str, &str_pos, &str_size, name + 1, cache, tree, 0, print,
			&link, linkpos, linksize
		);
	else
		my_dump_var_int(
			sv, &str, &str_pos, &str_size, name, cache, tree, 0, print,
			&link, linkpos, linksize
		);
	if( print )
		str[str_pos ++] = '\n';
	str[str_pos] = '\0';
	if( rlen != NULL )
		(*rlen) = str_pos;
	Safefree( link );
	return str;
}

/*****************************************************************************
 * common functions
 *****************************************************************************/

INLINE char *my_strncpy( char *dst, const char *src, size_t len ) {
	for( ; len > 0 && *src != '\0'; len --, *dst ++ = *src ++ );
	*dst = '\0';
	return dst;
}

INLINE char *my_strcpy( char *dst, const char *src ) {
	for( ; *src != '\0'; *dst ++ = *src ++ );
	*dst = '\0';
	return dst;
}

INLINE char *my_strcpyu( char *dst, const char *src ) {
	for( ; *src != '\0'; *dst ++ = toupper( *src ++ ) );
	*dst = '\0';
	return dst;
}

INLINE char *my_strcpyl( char *dst, const char *src ) {
	for( ; *src != '\0'; *dst ++ = tolower( *src ++ ) );
	*dst = '\0';
	return dst;
}

INLINE char *my_itoa( char *str, long value, int radix ) {
    char tmp[21], *ret = tmp, neg = 0;
	if( value < 0 ) {
		value = -value;
		neg = 1;
	}
	switch( radix ) {
	case 16:
		do {
			*ret ++ = HEXTAB[value % 16];
			value /= 16;
		} while( value > 0 );
		break;
	default:
		do {
			*ret ++ = (char) ((value % radix) + '0');
			value /= radix;
		} while( value > 0 );
		if( neg )
			*ret ++ = '-';
	}
	for( ret --; ret >= tmp; *str ++ = *ret, ret -- );
	*str = '\0';
	return str;
}

INLINE char *my_ltoa( char *str, xlong value, int radix ) {
    char tmp[21], *ret = tmp, neg = 0;
	if( value < 0 ) {
		value = -value;
		neg = 1;
	}
	switch( radix ) {
	case 16:
		do {
			*ret ++ = HEXTAB[value % 16];
			value /= 16;
		} while( value > 0 );
		break;
	default:
		do {
			*ret ++ = (char) ((value % radix) + '0');
			value /= radix;
		} while( value > 0 );
		if( neg )
			*ret ++ = '-';
	}
	for( ret --; ret >= tmp; *str ++ = *ret, ret -- );
	*str = '\0';
	return str;
}

INLINE char *my_ultoa( char *str, u_xlong value, int radix ) {
    char tmp[20], *ret = tmp;
	switch( radix ) {
	case 16:
		do {
			*ret ++ = HEXTAB[value % 16];
			value /= 16;
		} while( value > 0 );
		break;
	default:
		do {
			*ret ++ = (char) ((value % radix) + '0');
			value /= radix;
		} while( value > 0 );
	}
	for( ret --; ret >= tmp; *str ++ = *ret, ret -- );
	*str = '\0';
	return str;
}

INLINE char *my_ftoa( register char *buf, double f ) {
	register char *p;
	f = (f * 1e9 + (f < 0.0 ? -0.5 : 0.5)) / 1e9;
	if( f < 0.0 && f > -1.0 ) {
		p = my_itoa( buf, -1, 10 );
		*(p - 1) = '0';
	}
	else
		p = my_itoa( buf, (long) f, 10 );
	*p ++ = '.';
	if( f < 0.0 )
		f = -f;
	p = my_itoa( p, (long) ((f - (long) f) * 1e9), 10 );
	// remove tailing zeros
	for( ; *(p - 1) == '0'; p -- );
	*p = '\0';
	return p;
}

INLINE int my_stricmp( const char *cs, const char *ct ) {
	register signed char res;
	while( 1 ) {
		if( (res = toupper( *cs ) - toupper( *ct ++ )) != 0 || ! *cs ++ )
			break;
	}
	return res;
}

INLINE int my_strnicmp( const char *cs, const char *ct, size_t len ) {
	register signed char res = 0;
	register const char *ce = cs + len;
	while( 1 ) {
		if( cs == ce || ! *cs || (res = toupper( *cs ) - toupper( *ct )) != 0 )
			break;
		ct ++, cs ++;
	}
	return res;
}

INLINE char *my_stristr( const char *str1, const char *str2 ) {
	char *pptr, *sptr, *start;
	for( start = (char *) str1; *start != '\0'; start ++ ) {
		/* find start of pattern in string */
		for( ; *start != '\0' && toupper( *start ) != toupper( *str2 ); start ++ );
		if( *start == '\0' )
			return NULL;
		pptr = (char *) str2;
		sptr = (char *) start;
		while( toupper( *sptr ) == toupper( *pptr ) ) {
			sptr ++;
			pptr ++;
			/* if end of pattern then pattern was found */
			if( *pptr == '\0' )
				return start;
		}
	}
	return NULL;
}

INLINE int my_utf8_to_unicode(
	const char *src, size_t src_len, char *dst, size_t dst_max
) {
	unsigned int c, wc;
	const char *se;
	char *de = dst + dst_max, *ds = dst;
	if( src_len == 0 )
		src_len = strlen( src );
	se = src + src_len;
	while( src < se && dst < de ) {
		c = *src ++;
		if( c & 0x80 ) {
			if( src >= se )
				return -1;
			c &= 0x3F;
			if( c & 0x20 ) {
				wc = *src ++;
				if( (wc & 0xC0) != 0x80 || src >= se )
					return -1;
				c = (c << 6) | (wc & 0x3F);
			}
			wc = *src ++;
			if( (wc & 0xC0) != 0x80 )
				return -1;
			wc = (c << 6) | (wc & 0x3F);
			if( dst + 2 > de )
				break;
			*dst ++ = (char) (wc >> 8);
			*dst ++ = (char) wc;
		}
		else {
			if( dst + 2 > de )
				break;
			*dst ++ = 0;
			*dst ++ = (char) c;
		}
	}
	*dst = '\0';
	return (int) (dst - ds);
}

INLINE int my_unicode_to_utf8(
	const char *src, size_t src_len, char *dst, size_t dst_max
) {
	unsigned int c;
	const char *se;
	char *de = dst + dst_max, *ds = dst;
	if( src_len == 0 )
		src_len = strlen( src );
	if( src_len % 2 )
		return -1;
	se = src + src_len - 1;
	while( src < se && dst < de ) {
		c = (src[0] << 8) | src[1];
		src += 2;
		if( c <= 0x7F ) {
			*dst ++ = (char) c;
		}
		else
		if( c > 0x7FF ) {
			if( dst + 3 > de )
				break;
			*dst ++ = (char) (0xE0 | (c >> 12));
			*dst ++ = (char) (0x80 | ((c >> 6) & 0x3F));
			*dst ++ = (char) (0x80 | (c & 0x3F));
		}
		else {
			if( dst + 2 > de )
				break;
			*dst ++ = (char) (0xC0 | (c >> 6));
			*dst ++ = (char) (0x80 | (c & 0x3F));
		}
	}
	*dst = '\0';
	return (int) (dst - ds);
}

INLINE int my_utf8_toupper(
	const char *src, size_t src_len, char *dst, size_t dst_len
) {
	unsigned int c, wc;
	const char *se;
	char *de = dst + dst_len, *ds = dst;

	if( src_len == 0 )
		src_len = strlen( src );
	se = src + src_len;

	while( src < se && dst < de ) {
		c = *src ++;
		if( c & 0x80 ) {
			/* convert utf-8 to unicode */
			if( src >= se )
				return -1;
			c &= 0x3F;
			if( c & 0x20 ) {
				wc = *src ++;
				if( (wc & 0xC0) != 0x80 || src >= se )
					return -1;
				c = (c << 6) | (wc & 0x3F);
			}
			wc = *src ++;
			if( (wc & 0xC0) != 0x80 )
				return -1;

			/* change case of unicode character */
			wc = my_towupper( (c << 6) | (wc & 0x3F) );

			/* convert back to utf-8 */
			if( wc > 0x7FF ) {
				if( dst + 3 > de )
					break;
				*dst ++ = (char) (0xE0 | (wc >> 12));
				*dst ++ = (char) (0x80 | ((wc >> 6) & 0x3F));
				*dst ++ = (char) (0x80 | (wc & 0x3F));
			}
			else {
				if( dst + 2 > de )
					break;
				*dst ++ = (char) (0xC0 | (wc >> 6));
				*dst ++ = (char) (0x80 | (wc & 0x3F));
			}
		}
		else {
			/* ansi char */
			*dst ++ = toupper( c & 0xFF );
		}
	}
	*dst = '\0';
	return (int) (dst - ds);
}

INLINE int my_utf8_tolower(
	const char *src, size_t src_len, char *dst, size_t dst_len
) {
	unsigned int c, wc;
	const char *se;
	char *de = dst + dst_len, *ds = dst;

	if( src_len == 0 )
		src_len = strlen( src );
	se = src + src_len;

	while( src < se && dst < de ) {
		c = *src ++;
		if( c & 0x80 ) {
			/* convert utf-8 to unicode */
			if( src >= se )
				return -1;
			c &= 0x3F;
			if( c & 0x20 ) {
				wc = *src ++;
				if( (wc & 0xC0) != 0x80 || src >= se )
					return -1;
				c = (c << 6) | (wc & 0x3F);
			}
			wc = *src ++;
			if( (wc & 0xC0) != 0x80 )
				return -1;

			/* change case of unicode character */
			wc = my_towlower( (c << 6) | (wc & 0x3F) );

			/* convert back to utf-8 */
			if( wc > 0x7FF ) {
				if( dst + 3 > de )
					break;
				*dst ++ = (char) (0xE0 | (wc >> 12));
				*dst ++ = (char) (0x80 | ((wc >> 6) & 0x3F));
				*dst ++ = (char) (0x80 | (wc & 0x3F));
			}
			else {
				if( dst + 2 > de )
					break;
				*dst ++ = (char) (0xC0 | (wc >> 6));
				*dst ++ = (char) (0x80 | (wc & 0x3F));
			}
		}
		else {
			/* ansi char */
			*dst ++ = tolower( c & 0xFF );
		}
	}
	*dst = '\0';
	return (int) (dst - ds);
}

INLINE unsigned int my_towupper( unsigned int c )
{
	if( c < 0x0100 ) {
		if( c == 0x00b5 )
	    	return 0x039c;
		if( (c >= 0x00e0 && c <= 0x00fe) ||
			(c >= 0x0061 && c <= 0x007a)
		) {
			return (c - 0x0020);
		}
	  	if( c == 0x00ff )
	    	return 0x0178;
	  	return c;
	}
	else
	if( c < 0x0300 ) {
		if( (c >= 0x0101 && c <= 0x012f) ||
			(c >= 0x0133 && c <= 0x0137) ||
			(c >= 0x014b && c <= 0x0177) ||
			(c >= 0x01df && c <= 0x01ef) ||
			(c >= 0x01f9 && c <= 0x021f) ||
			(c >= 0x0223 && c <= 0x0233)
		) {
			if( c & 0x0001 )
				return (c - 1);
			return c;
		}

		if( (c >= 0x013a && c <= 0x0148) ||
			(c >= 0x01ce && c <= 0x1dc)
		) {
			if( ! (c & 0x0001) )
				return (c - 1);
			return c;
		}
	  
		if( c == 0x0131 )
			return 0x0049;
		
		if( c == 0x017a || c == 0x017c || c == 0x017e )
	    	return (c - 1);
	  
		if( c >= 0x017f && c <= 0x0292 ) {
			switch( c ) {
				case 0x017f:	return 0x0053;
				case 0x0183:	return 0x0182;
				case 0x0185:	return 0x0184;
				case 0x0188:	return 0x0187;
				case 0x018c:	return 0x018b;
				case 0x0192:	return 0x0191;
				case 0x0195:	return 0x01f6;
				case 0x0199:	return 0x0198;
				case 0x019e:	return 0x0220;
	
				case 0x01a1:	return 0x01a0;
				case 0x01a3:	return 0x01a2;
				case 0x01a5:	return 0x01a4;
				case 0x01a8:	return 0x01a7;
				case 0x01ad:	return 0x01ac;
				case 0x01b0:	return 0x01af;
				case 0x01b4:	return 0x01b3;
				case 0x01b6:	return 0x01b5;
				case 0x01b9:	return 0x01b8;
				case 0x01bd:	return 0x01bc;
				case 0x01c5:	return 0x01c4;
				case 0x01c8:	return 0x01c7;
				case 0x01cb:	return 0x01ca;
				case 0x01f2:	return 0x01f1;
				case 0x01f5:	return 0x01f4;
	
				case 0x01bf:	return 0x01f7;
	
				case 0x01c6:	return 0x01c4;
				case 0x01c9:	return 0x01c7;
				case 0x01cc:	return 0x01ca;
	
				case 0x01dd:	return 0x018e;
				case 0x01f3:	return 0x01f1;
				case 0x0253:	return 0x0181;
				case 0x0254:	return 0x0186;
				case 0x0256:	return 0x0189;
				case 0x0257:	return 0x018a;
				case 0x0259:	return 0x018f;
				case 0x025b:	return 0x0190;
				case 0x0260:	return 0x0193;
				case 0x0263:	return 0x0194;
				case 0x0268:	return 0x0197;
				case 0x0269:	return 0x0196;
				case 0x026f:	return 0x019c;
				case 0x0272:	return 0x019d;
				case 0x0275:	return 0x019f;
				case 0x0280:	return 0x01a6;
				case 0x0283:	return 0x01a9;
				case 0x0288:	return 0x01ae;
				case 0x028a:	return 0x01b1;
				case 0x028b:	return 0x01b2;
				case 0x0292:	return 0x01b7;
			}
		}
	}
	else
	if( c < 0x0400 ) {
		if( c == 0x03ac )
			return 0x0386;
	  
		if( (c & 0xfff0) == 0x03a0 && c >= 0x03ad )
			return (c - 0x0015);
		
		if( c >= 0x03b1 && c <= 0x03cb && c != 0x03c2 )
			return (c - 0x0020);
	  
		if( c == 0x03c2 )
			return 0x03a3;
	  
		if( c >= 0x03cc && c <= 0x03f5 ) {
			switch( c ) {
				case 0x03cc:	return 0x038c;
				case 0x03cd:	return 0x038e;
				case 0x03ce:	return 0x038f;
	
				case 0x03d0:	return 0x0392;
				case 0x03d1:	return 0x0398;
				case 0x03d5:	return 0x03a6;
				case 0x03d6:	return 0x03a0;
	
				case 0x03d9:	return 0x03d8;
				case 0x03db:	return 0x03da;
				case 0x03dd:	return 0x03dc;
				case 0x03df:	return 0x03de;
				case 0x03e1:	return 0x03e0;
				case 0x03e3:	return 0x03e2;
				case 0x03e5:	return 0x03e4;
				case 0x03e7:	return 0x03e6;
				case 0x03e9:	return 0x03e8;
				case 0x03eb:	return 0x03ea;
				case 0x03ed:	return 0x03ec;
				case 0x03ef:	return 0x03ee;
	
				case 0x03f0:	return 0x039a;
				case 0x03f1:	return 0x03a1;
				case 0x03f2:	return 0x03a3;
				case 0x03f5:	return 0x0395;
			}
		}
	}
	else
	if( c < 0x0500 ) {

		if( c >= 0x0450 && c <= 0x045f )
			return (c - 0x0050);

		if( c >= 0x0430 && c <= 0x044f )
			return (c - 0x0020);

		if( (c >= 0x0461 && c <= 0x0481) ||
			(c >= 0x048b && c <= 0x04bf) ||
			(c >= 0x04d1 && c <= 0x04f5)
		) {
			if( c & 0x0001 )
				return (c - 1);
			return c;
		}

		if( c >= 0x04c2 && c <= 0x04ce ) {
			if( ! (c & 0x0001) )
				return (c - 1);
			return c;
		}

		if( c == 0x04f9 )
			return 0x04f8;
	}
	else
	if( c < 0x1f00 ) {
		if( (c >= 0x0501 && c <= 0x050f) ||
			(c >= 0x1e01 && c <= 0x1e95) ||
			(c >= 0x1ea1 && c <= 0x1ef9)
		) {
			if( c & 0x0001 )
				return (c - 1);
			return c;
		}

		if( c >= 0x0561 && c <= 0x0586 )
			return (c - 0x0030);

		if( c == 0x1e9b )
			return 0x1e60;
	}
	else
	if( c < 0x2000 ) {
		if( (c >= 0x1f00 && c <= 0x1f07) ||
			(c >= 0x1f10 && c <= 0x1f15) ||
			(c >= 0x1f20 && c <= 0x1f27) ||
			(c >= 0x1f30 && c <= 0x1f37) ||
			(c >= 0x1f40 && c <= 0x1f45) ||
			(c >= 0x1f60 && c <= 0x1f67) ||
			(c >= 0x1f80 && c <= 0x1f87) ||
			(c >= 0x1f90 && c <= 0x1f97) ||
			(c >= 0x1fa0 && c <= 0x1fa7)
		) {
			return (c + 0x0008);
		}

		if( c >= 0x1f51 && c <= 0x1f57 && (c & 0x0001) )
			return (c + 0x0008);

		if( c >= 0x1f70 && c <= 0x1ff3 ) {
			switch( c ) {
				case 0x1fb0:	return 0x1fb8;
				case 0x1fb1:	return 0x1fb9;
				case 0x1f70:	return 0x1fba;
				case 0x1f71:	return 0x1fbb;
				case 0x1fb3:	return 0x1fbc;
				case 0x1fbe:	return 0x0399;
				case 0x1f72:	return 0x1fc8;
				case 0x1f73:	return 0x1fc9;
				case 0x1f74:	return 0x1fca;
				case 0x1f75:	return 0x1fcb;
				case 0x1fd0:	return 0x1fd8;
				case 0x1fd1:	return 0x1fd9;
				case 0x1f76:	return 0x1fda;
				case 0x1f77:	return 0x1fdb;
				case 0x1fe0:	return 0x1fe8;
				case 0x1fe1:	return 0x1fe9;
				case 0x1f7a:	return 0x1fea;
				case 0x1f7b:	return 0x1feb;
				case 0x1fe5:	return 0x1fec;
				case 0x1f78:	return 0x1ff8;
				case 0x1f79:	return 0x1ff9;
				case 0x1f7c:	return 0x1ffa;
				case 0x1f7d:	return 0x1ffb;
				case 0x1ff3:	return 0x1ffc;
			}
		}
	}
	else {
		if( c >= 0x2170 && c <= 0x217f )
			return (c - 0x0010);
	  
		if( c >= 0x24d0 && c <= 0x24e9 )
			return (c - 0x001a);
		
		if( c >= 0xff41 && c <= 0xff5a )
			return (c - 0x0020);
		
		if( c >= 0x10428 && c <= 0x1044d )
			return (c - 0x0028);
	}
  
	return (c < 0x00ff ? (unsigned int) (toupper( (int) c )) : c);
}

INLINE unsigned int my_towlower( unsigned int c )
{
	if( c < 0x0100 ) {
		if( (c >= 0x0041 && c <= 0x005a) ||
			(c >= 0x00c0 && c <= 0x00de)
		) {
    		return (c + 0x20);
    	}

		if( c == 0x00b5 )
    		return 0x03bc;

		return c;
	}
	else
	if( c < 0x0300 ) {
		if( (c >= 0x0100 && c <= 0x012e) ||
			(c >= 0x0132 && c <= 0x0136) ||
			(c >= 0x014a && c <= 0x0176) ||
			(c >= 0x01de && c <= 0x01ee) ||
			(c >= 0x01f8 && c <= 0x021e) ||
			(c >= 0x0222 && c <= 0x0232)
		) {
			if( ! (c & 0x01) )
				return (c + 1);
			return c;
		}

		if( (c >= 0x0139 && c <= 0x0147) ||
			(c >= 0x01cd && c <= 0x91db)
		) {
			if( c & 0x0001 )
				return (c + 1);
			return c;
		}
  
		if( c >= 0x0178 && c <= 0x01f7 ) {
			switch( c ) {
				case 0x0178:	return 0x00ff;
					
				case 0x0179:	return 0x017a;
				case 0x017b:	return 0x017c;
				case 0x017d:	return 0x017e;
				case 0x0182:	return 0x0183;
				case 0x0184:	return 0x0185;
				case 0x0187:	return 0x0188;
				case 0x018b:	return 0x018c;
				case 0x0191:	return 0x0192;
				case 0x0198:	return 0x0199;
				case 0x01a0:	return 0x01a1;
				case 0x01a2:	return 0x01a3;
				case 0x01a4:	return 0x01a5;
				case 0x01a7:	return 0x01a8;
				case 0x01ac:	return 0x01ad;
				case 0x01af:	return 0x01b0;
				case 0x01b3:	return 0x01b4;
				case 0x01b5:	return 0x01b6;
				case 0x01b8:	return 0x01b9;
				case 0x01bc:	return 0x01bd;
				case 0x01c5:	return 0x01c6;
				case 0x01c8:	return 0x01c9;
				case 0x01cb:	return 0x01cc;
				case 0x01cd:	return 0x01ce;
				case 0x01cf:	return 0x01d0;
				case 0x01d1:	return 0x01d2;
				case 0x01d3:	return 0x01d4;
				case 0x01d5:	return 0x01d6;
				case 0x01d7:	return 0x01d8;
				case 0x01d9:	return 0x01da;
				case 0x01db:	return 0x01dc;
				case 0x01f2:	return 0x01f3;
				case 0x01f4:	return 0x01f5;
				
				case 0x017f:	return 0x0073;
				case 0x0181:	return 0x0253;
				case 0x0186:	return 0x0254;
				case 0x0189:	return 0x0256;
				case 0x018a:	return 0x0257;
				case 0x018e:	return 0x01dd;
				case 0x018f:	return 0x0259;
				case 0x0190:	return 0x025b;
				case 0x0193:	return 0x0260;
				case 0x0194:	return 0x0263;
				case 0x0196:	return 0x0269;
				case 0x0197:	return 0x0268;
				case 0x019c:	return 0x026f;
				case 0x019d:	return 0x0272;
				case 0x019f:	return 0x0275;
				case 0x01a6:	return 0x0280;
				case 0x01a9:	return 0x0283;
				case 0x01ae:	return 0x0288;
				case 0x01b1:	return 0x028a;
				case 0x01b2:	return 0x028b;
				case 0x01b7:	return 0x0292;

				case 0x01c4:	return 0x01c6;
				case 0x01c7:	return 0x01c9;
				case 0x01ca:	return 0x01cc;
				case 0x01f1:	return 0x01f3;

				case 0x01f6:	return 0x0195;
				case 0x01f7:	return 0x01bf;
			}
		}

		if( c == 0x0220 )
			return 0x019e;
	}
	else
	if( c < 0x0400 ) {

		if( c >= 0x0391 && c <= 0x03ab && c != 0x03a2 )
			return (c + 0x20);

		if( c >= 0x03d8 && c <= 0x03ee && ! (c & 0x0001) )
			return (c + 1);

		if( c >= 0x0386 && c <= 0x03f5 ) {
			switch( c ) {
				case 0x0386:	return 0x03ac;
				case 0x0388:	return 0x03ad;
				case 0x0389:	return 0x03ae;
				case 0x038a:	return 0x03af;
				case 0x038c:	return 0x03cc;
				case 0x038e:	return 0x03cd;
				case 0x038f:	return 0x038f;
				case 0x03c2:	return 0x03c3;
				case 0x03d0:	return 0x03b2;
				case 0x03d1:	return 0x03b8;
				case 0x03d5:	return 0x03c6;
				case 0x03d6:	return 0x03c0;
				case 0x03f0:	return 0x03ba;
				case 0x03f1:	return 0x03c1;
				case 0x03f2:	return 0x03c3;
				case 0x03f4:	return 0x03b8;
				case 0x03f5:	return 0x03b5;
			}
		}

		if( c == 0x0345 )
			return 0x03b9;
	}
	else
	if( c < 0x0500 ) {
		
		if( c >= 0x0400 && c <= 0x040f )
			return (c + 0x0050);

		if( c >= 0x0410 && c <= 0x042f )
			return (c + 0x0020);
  
		if( (c >= 0x0460 && c <= 0x0480) ||
			(c >= 0x048a && c <= 0x04be) ||
			(c >= 0x04d0 && c <= 0x04f4) ||
			(c == 0x04f8)
		) {
			if( ! (c & 0x0001) )
				return (c + 1);
			return c;
		}

		if( c >= 0x04c1 && c <= 0x04cd ) {
			if( c & 0x0001 )
				return (c + 1);
			return c;
		}
	}
	else
	if( c < 0x1f00 ) {

		if( (c >= 0x0500 && c <= 0x050e) ||
			(c >= 0x1e00 && c <= 0x1e94) ||
			(c >= 0x1ea0 && c <= 0x1ef8)
		) {
			if( ! (c & 0x0001) )
				return (c + 1);
			return c;
		}
  
		if( c >= 0x0531 && c <= 0x0556 )
			return (c + 0x0030);

		if( c == 0x1e9b )
			return 0x1e61;
	}
	else
	if( c < 0x2000 ) {

		if( (c >= 0x1f08 && c <= 0x1f0f) ||
			(c >= 0x1f18 && c <= 0x1f1d) ||
			(c >= 0x1f28 && c <= 0x1f2f) ||
			(c >= 0x1f38 && c <= 0x1f3f) ||
			(c >= 0x1f48 && c <= 0x1f4d) ||
			(c >= 0x1f68 && c <= 0x1f6f) ||
			(c >= 0x1f88 && c <= 0x1f8f) ||
			(c >= 0x1f98 && c <= 0x1f9f) ||
			(c >= 0x1fa8 && c <= 0x1faf)
		) {
			return (c - 0x0008);
		}

		if( c >= 0x1f59 && c <= 0x1f5f ) {
			if( c & 0x01)
				return (c - 0x08);
			return c;
		}

		if( c >= 0x1fb8 && c <= 0x1ffc ) {
			switch( c ) {
				case 0x1fb8:	return 0x1fb0;
				case 0x1fb9:	return 0x1fb1;
				case 0x1fd8:	return 0x1fd0;
				case 0x1fd9:	return 0x1fd1;
				case 0x1fe8:	return 0x1fe0;
				case 0x1fe9:	return 0x1fe1;

				case 0x1fba:	return 0x1f70;
				case 0x1fbb:	return 0x1f71;

				case 0x1fbc:	return 0x1fb3;
				case 0x1fbe:	return 0x03b9;

				case 0x1fc8:	return 0x1f72;
				case 0x1fc9:	return 0x1f73;
				case 0x1fca:	return 0x1f74;
				case 0x1fcb:	return 0x1f75;

				case 0x1fcc:	return 0x1fc3;

				case 0x1fda:	return 0x1f76;
				case 0x1fdb:	return 0x1f77;

				case 0x1fea:	return 0x1f7a;
				case 0x1feb:	return 0x1f7b;

				case 0x1fec:	return 0x1fe5;

				case 0x1ffa:	return 0x1f7c;
				case 0x1ffb:	return 0x1f7d;

				case 0x1ffc:	return 0x1ff3;
			}
		}
	}
	else {

		if( c >= 0x2160 && c <= 0x216f )
			return (c + 0x0010);
  
		if( c >= 0x24b6 && c <= 0x24cf )
			return (c + 0x001a);
  
		if( c >= 0xff21 && c <= 0xff3a )
			return (c + 0x0020);
  
		if( c >= 0x10400 && c <= 0x10425 )
			return (c + 0x0028);

		if( c == 0x2126 )
			return 0x03c9;
		if( c == 0x212a )
			return 0x006b;
		if( c == 0x212b )
			return 0x00e5;
	}
  
	return (c < 0x00ff ? (unsigned int) (tolower( (int) c )) : c);
}


#ifdef DBE_DEBUG

INLINE int my_debug( const char *fmt, ... ) {
	va_list a;
	int r;
	size_t l;
	char *tmp, *s1;
	l = strlen( fmt );
	tmp = malloc( 64 + l );
	s1 = my_strcpy( tmp, "[DBE] " );
	s1 = my_strcpy( s1, fmt );
	va_start( a, fmt );
	r = vfprintf( stderr, tmp, a );
	fflush( stderr );
	va_end( a );
	free( tmp );
	return r;
}

#if DBE_DEBUG > 1

HV					*hv_dbg_mem = NULL;
perl_mutex			dbg_mem_lock;

void debug_init() {
	_debug( "init memory debugger\n" );
	MUTEX_INIT( &dbg_mem_lock );
	hv_dbg_mem = newHV();
}

void debug_free() {
	SV *sv_val;
	char *key, *val;
	I32 retlen;
	STRLEN lval;
	_debug( "hv_dbg_mem entries %u\n", HvKEYS( hv_dbg_mem ) );
	if( HvKEYS( hv_dbg_mem ) ) {
		hv_iterinit( hv_dbg_mem );
		while( (sv_val = hv_iternextsv( hv_dbg_mem, &key, &retlen )) != NULL ) {
			val = SvPV( sv_val, lval );
			_debug( "unfreed memory from %s\n", val );
		}
	}
	sv_2mortal( (SV *) hv_dbg_mem );
	MUTEX_DESTROY( &dbg_mem_lock );
}

#endif

#endif
