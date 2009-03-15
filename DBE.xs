#include "dbe_def.h"

/*
void _BOOT() { ***ue anchor*** }
*/

MODULE = DBE		PACKAGE = DBE		PREFIX = _DBE_

BOOT:
{
#ifdef DBE_DEBUG
	_debug( "booting DBE v%s build %u\n", XS_VERSION, DBE_BUILD );
#if DBE_DEBUG > 1
	debug_init();
#endif
#endif
	Zero( &global, 1, global );
	global.sv_trace_out = newRV( (SV *) PL_stderrgv );
#ifdef USE_ITHREADS
	MUTEX_INIT( &global.thread_lock );
#endif
}

#/*****************************************************************************
# * _DBE_CLONE()
# *****************************************************************************/

#ifdef USE_ITHREADS

void
_DBE_CLONE( ... )
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res;
	dbe_stmt_t *stmt;
	dbe_lob_stream_t *ls;
PPCODE:
	GLOBAL_LOCK();
	for( con = global.first_con; con != NULL; con = con->next ) {
#ifdef DBE_DEBUG
		_debug( "DBE::CLONE con %lu, ref: %d\n", con->id, con->refcnt );
#endif
		con->refcnt ++;
		for( stmt = con->first_stmt; stmt != NULL; stmt = stmt->next ) {
#ifdef DBE_DEBUG
			_debug( "DBE::CLONE stmt %lu, ref: %d\n", stmt->id, stmt->refcnt );
#endif
			stmt->refcnt ++;
		}
		for( res = con->first_res; res != NULL; res = res->next ) {
#ifdef DBE_DEBUG
			_debug( "DBE::CLONE res %lu, ref: %d\n", res->id, res->refcnt );
#endif
			res->refcnt ++;
		}
		for( ls = con->first_lob; ls != NULL; ls = ls->next ) {
#ifdef DBE_DEBUG
			_debug( "DBE::CLONE lob %lu, ref: %d\n", ls->id, ls->refcnt );
#endif
			ls->refcnt ++;
		}
	}
	GLOBAL_UNLOCK();

#endif

#/*****************************************************************************
# * _DBE_END()
# *****************************************************************************/

void
_DBE_END( ... )
CODE:
	if( items ) {} /* avoid compiler warning */
#ifdef DBE_DEBUG
	_debug( "DBE::END called\n" );
#endif
	IF_TRACE_GLOBAL( DBE_TRACE_ALL )
		TRACE( NULL, DBE_TRACE_ALL, FALSE, "END reached\n" );
	dbe_free();
#ifdef USE_ITHREADS
	MUTEX_DESTROY( &global.thread_lock );
#endif


#/*****************************************************************************
# * _DBE___cleanup__()
# *****************************************************************************/

void
_DBE___cleanup__( ... )
CODE:
	if( items ); /* avoid compiler warning */
#ifdef DBE_DEBUG
	_debug( "DBE::__cleanup__ called\n" );
#endif
	dbe_cleanup();


void
_DBE_connect( ... )
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	SV *sv;
	HV *hv;
	STRLEN lkey, lval, ldriver = 5;
	char tmp[256], **args, *key, *val = NULL, *dsn = NULL;
	char error[1024];
	const char *driver = "Text";
	int argc = 0, i;
	unsigned int flags;
	enum dbe_getinfo_type gi_type;
PPCODE:
	Newxz( con, 1, dbe_con_t );
	con->flags = CON_ONERRORDIE;
	if( PL_dowarn )
		con->flags |= CON_ONERRORWARN;
	con->sv_trace_out = SvREFCNT_inc( global.sv_trace_out );
	con->trace_level = global.trace_level;
	if( items < 1 )
		Perl_croak( aTHX_ "Usage: DBE->connect(...)" );
	if( items == 2 ) {
		/* use DSN */
		key = SvPV( ST(1), lkey );
		Newx( dsn, lkey + 1, char );
		Copy( key, dsn, lkey + 1, char );
		argc = dbe_dsn_parse( dsn, &args );
		for( i = 0; i < argc; i += 2 ) {
			if( my_stricmp( args[i], "driver" ) == 0 ||
				my_stricmp( args[i], "provider" ) == 0
			) {
				driver = args[i + 1];
				ldriver = strlen( driver );
			}
			else if( my_stricmp( args[i], "reconnect" ) == 0 ) {
				con->reconnect_count = atol( args[i + 1] );
			}
			else if( my_stricmp( args[i], "croak" ) == 0 ) {
				if( STR_TRUE( args[i + 1] ) )
					con->flags |= CON_ONERRORDIE;
				else
					con->flags &= (~CON_ONERRORDIE);
			}
			else if( my_stricmp( args[i], "warn" ) == 0 ) {
				if( STR_TRUE( args[i + 1] ) )
					con->flags |= CON_ONERRORWARN;
				else
					con->flags &= (~CON_ONERRORWARN);
			}
		}
	}
	else {
		Newx( args, items - 1, char * );
		for( i = 1; i < items - 1; i += 2 ) {
			if( ! SvPOK( ST(i) ) )
				continue;
			key = SvPV( ST(i), lkey );
			args[i - 1] = key;
			if( ! SvOK( ST(i + 1) ) )
				args[i] = NULL;
			else {
				val = SvPV( ST(i + 1), lval );
				args[i] = val;
			}
			argc += 2;
			if( my_stricmp( key, "driver" ) == 0 ||
				my_stricmp( key, "provider" ) == 0
			) {
				driver = val;
				ldriver = lval;
			}
			else if( my_stricmp( key, "reconnect" ) == 0 ) {
				con->reconnect_count = (u_int) SvUV( ST(i + 1) );
			}
			else if( my_stricmp( key, "croak" ) == 0 ) {
				if( STR_TRUE( SvPV_nolen( ST(i + 1) ) ) )
					con->flags |= CON_ONERRORDIE;
				else
					con->flags &= (~CON_ONERRORDIE);
			}
			else if( my_stricmp( key, "warn" ) == 0 ) {
				if( STR_TRUE( SvPV_nolen( ST(i + 1) ) ) )
					con->flags |= CON_ONERRORWARN;
				else
					con->flags &= (~CON_ONERRORWARN);
			}
		}
	}
	IF_TRACE_GLOBAL( DBE_TRACE_ALL )
		TRACE( NULL, DBE_TRACE_ALL, TRUE, "-> connect(Driver.%s)", driver );
	key = my_strcpy( tmp, "DBE::Driver::" );
	key = my_strcpyu( key, driver );
	key = my_strcpy( key, "::VERSION" );
	if( get_sv( tmp, FALSE ) == NULL ) {
		/* load driver module */
		key = my_strcpy( tmp, "require DBE::Driver::" );
		key = my_strcpyu( key, driver );
#ifdef DBE_DEBUG
		_debug( "load driver module %s\n", tmp + 8 );
#endif
		IF_TRACE_GLOBAL( DBE_TRACE_ALL )
			TRACE( NULL, DBE_TRACE_ALL, FALSE,
				"   load driver module %s", tmp + 8 );
		eval_sv(
			sv_2mortal( newSVpvn( tmp, key - tmp ) ),
			G_DISCARD
		);
		if( SvTRUE( ERRSV ) ) {
			dbe_drv_set_error( NULL, -1, SvPV_nolen( ERRSV ) );
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE,
					"   init driver %s: %s", driver, global.last_error );
			sprintf( error,
				"Init driver %s: %s", driver, global.last_error );
			goto _error;
		}
	}
	con->drv = dbe_init_driver( driver, ldriver );
	if( con->drv == NULL ) {
#ifdef DBE_DEBUG
		_debug( "init driver failed: %s\n", global.last_error );
#endif
		IF_TRACE_GLOBAL( DBE_TRACE_ALL )
			TRACE( NULL, DBE_TRACE_ALL, FALSE,
				"   init driver %s: %s", driver, global.last_error );
		sprintf( error,
			"Init driver %s: %s", driver, global.last_error );
		goto _error;
	}
	dd = con->drv->def;
	if( dd->drv_connect == NULL ) {
		dbe_drv_set_error( NULL, -1, NOFUNCTION_ERROR, "connect" );
		sprintf( error,
			"[%s] Connect failed: %s", dd->description, global.last_error );
		goto _error;
	}
	con->drv_data = dd->drv_connect( con, args, argc );
	if( con->drv_data == NULL ) {
#ifdef DBE_DEBUG
		_debug( "connection failed: %s\n", con->last_error );
#endif
		IF_TRACE_GLOBAL( DBE_TRACE_ALL )
			TRACE( NULL, DBE_TRACE_ALL, FALSE,
				" <- connect failed: %s", con->last_error );
		global.last_errno = con->last_errno;
		my_strcpy( global.last_error, con->last_error );
		sprintf( error,
			"[%s] Connect failed: %s", dd->description, con->last_error );
		goto _error;
	}
	if( dd->con_set_trace != NULL )
		dd->con_set_trace( con->drv_data, global.trace_level );
	/* get some info */
	con->catalog_separator = '.';
	con->quote_id_prefix = '"';
	con->quote_id_suffix = '"';
	con->escape_pattern = '\\';
	con->catalog_location = SQL_CL_START;
	if( dd->con_getinfo != NULL ) {
		if( dd->con_getinfo(
			con->drv_data, SQL_IDENTIFIER_QUOTE_CHAR,
			tmp, sizeof(tmp), &i, &gi_type ) == DBE_OK
		) {
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE,
					"   getinfo(SQL_IDENTIFIER_QUOTE_CHAR)= '%c'", *tmp );
			con->quote_id_prefix = *tmp;
			con->quote_id_suffix = *tmp;
		}
		if( dd->con_getinfo(
			con->drv_data, SQL_SEARCH_PATTERN_ESCAPE,
			tmp, sizeof(tmp), &i, &gi_type ) == DBE_OK
		) {
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE,
					"   getinfo(SQL_SEARCH_PATTERN_ESCAPE)= '%c'", *tmp );
			con->escape_pattern = *tmp;
		}
		if( dd->con_getinfo(
			con->drv_data, SQL_CATALOG_NAME_SEPARATOR,
			tmp, sizeof(tmp), &i, &gi_type ) == DBE_OK
		) {
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE,
					"   getinfo(SQL_CATALOG_NAME_SEPARATOR)= '%c'", *tmp );
			con->catalog_separator = *tmp;
		}
		if( dd->con_getinfo(
			con->drv_data, SQL_CATALOG_LOCATION,
			tmp, sizeof(tmp), &i, &gi_type ) == DBE_OK
		) {
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE,
					"   getinfo(SQL_CATALOG_LOCATION)= %d", *((short *) tmp) );
			con->catalog_location = (char) *((short *) tmp);
		}
	}
	/* add connection */
	dbe_con_add( con );
	/* create the class */
	hv = gv_stashpv( con->drv->class_con, TRUE );
#ifdef USE_ITHREADS
	MUTEX_INIT( &con->thread_lock );
#endif
	sv = sv_2mortal( newSViv( (IV) con->id ) );
	IF_TRACE_GLOBAL( DBE_TRACE_ALL )
		TRACE( NULL, DBE_TRACE_ALL, TRUE,
			"<- connected= %s=SCALAR(0x%lx)",
			con->drv->class_con, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	Safefree( args );
	Safefree( dsn );
	XSRETURN(1);
_error:
	flags = con->flags;
	Safefree( con );
	Safefree( args );
	Safefree( dsn );
	if( flags & CON_ONERRORDIE )
		Perl_croak( aTHX_ error );
	if( flags & CON_ONERRORWARN )
		Perl_warn( aTHX_ error );
	XSRETURN_EMPTY;


#/*****************************************************************************
# * _DBE_open_drivers()
# *****************************************************************************/

void
_DBE_open_drivers( ... )
PREINIT:
	dbe_drv_t *drv;
PPCODE:
	GLOBAL_LOCK();
	for( drv = global.first_drv; drv != NULL; drv = drv->next ) {
		XPUSHs( sv_2mortal( newSVpv( drv->name, 0 ) ) );
		XPUSHs( sv_2mortal( newSVpv( drv->def->description, 0 ) ) );
	}
	GLOBAL_UNLOCK();


#/*****************************************************************************
# * _DBE_installed_drivers()
# *****************************************************************************/

void
_DBE_installed_drivers( ... )
PREINIT:
	dbe_drv_t *drv;
	AV *inc;
	SV **psv, **svb = NULL;
	int il, i, svbl = 0;
	char *skey, tmp[1024], module[256], *s1;
	STRLEN lkey;
	DIR *dh;
	Direntry_t *de;
PPCODE:
	if( (inc = get_av( "INC", 0 )) == NULL )
		XSRETURN_EMPTY;
	il = av_len( inc );
	for( i = il; i >= 0; i -- ) {
		if( (psv = av_fetch( inc, i, 0 )) == NULL || ! SvPOK( (*psv) ) )
			continue;
		skey = SvPVx( (*psv), lkey );
		s1 = my_strcpy( tmp, skey );
		s1 = my_strcpy( s1, "/DBE/Driver/" );
		dh = PerlDir_open( tmp );
		if( dh == NULL )
			continue;
		while( (de = PerlDir_read( dh )) != NULL ) {
			if( de->d_name[0] == '.' )
				continue;
			my_strcpy( module, de->d_name );
			skey = my_stristr( module, ".PM" );
			if( skey == NULL )
				continue;
			*skey = '\0';
			s1 = my_strcpy( tmp, "require DBE::Driver::" );
			s1 = my_strcpy( s1, module );
#ifdef DBE_DEBUG
			_debug( "eval '%s'\n", tmp );
#endif
			IF_TRACE_GLOBAL( DBE_TRACE_ALL )
				TRACE( NULL, DBE_TRACE_ALL, FALSE, "-> eval('%s')", tmp );
			eval_sv(
				sv_2mortal( newSVpvn( tmp, s1 - tmp ) ),
				G_DISCARD | G_KEEPERR
			);
			if( SvTRUE( ERRSV ) ) {
				dbe_drv_set_error( NULL, -1, SvPV_nolen( ERRSV ) );
				continue;
			}
			drv = dbe_init_driver( module, skey - module );
			if( drv == NULL )
				continue;
			if( (svbl % 8) == 0 )
				Renew( svb, svbl + 8, SV * );
			svb[svbl ++] = sv_2mortal( newSVpv( drv->name, 0 ) );
			svb[svbl ++] = sv_2mortal( newSVpv( drv->def->description, 0 ) );
		}
		PerlDir_close( dh );
	}
	for( i = 0; i < svbl; i ++ )
		XPUSHs( svb[i] );
	Safefree( svb );


#/*****************************************************************************
# * _DBE_errno()
# *****************************************************************************/

void
_DBE_errno( ... )
PPCODE:
	XSRETURN_IV( global.last_errno );


#/*****************************************************************************
# * _DBE_error()
# *****************************************************************************/

void
_DBE_error( ... )
PPCODE:
	GLOBAL_LOCK();
	XPUSHs( sv_2mortal( newSVpvn(
		global.last_error, strlen( global.last_error ) ) ) );
	GLOBAL_UNLOCK();


#/*****************************************************************************
# * _DBE_trace()
# *****************************************************************************/

void
_DBE_trace( ... )
PREINIT:
	drv_def_t *dd;
	dbe_con_t *con;
PPCODE:
	/* Usage: DBE->trace( [level [, handle]] ) */
	GLOBAL_LOCK();
	if( items > 1 ) {
		if( SvIOK( ST(1) ) ) {
			global.trace_level = (int) SvIV( ST(1) );
		}
		else {
			global.trace_level =
				dbe_trace_level_by_name( SvPV_nolen( ST(1) ) );
		}
	}
	else {
		global.trace_level = DBE_TRACE_NONE;
	}
	for( con = global.first_con; con != NULL; con = con->next ) {
		dd = con->drv->def;
		if( ! con->trace_local && dd->con_set_trace != NULL )
			dd->con_set_trace( con->drv_data, global.trace_level );
	}
	if( global.trace_level <= DBE_TRACE_NONE )
		goto exit;
	if( items > 2 && SvOK( ST(2) ) ) {
		SvREFCNT_dec( global.sv_trace_out );
		global.sv_trace_out = SvREFCNT_inc( ST(2) );
	}
	dbe_perl_print( global.sv_trace_out, "\n", 1 );
	TRACE( NULL, 0, TRUE,
		"DBE v%s build %u %s trace level set to %s [%d]",
		XS_VERSION, DBE_BUILD,
#ifdef USE_ITHREADS
		"thread-multi",
#else
		"",
#endif
		dbe_trace_name_by_level( global.trace_level ),
		global.trace_level
	);
	if( ! PL_dowarn ) {
		dbe_perl_print( global.sv_trace_out,
			"    Note: Perl is running without the -w option\n", 48
		);
	}
exit:
	GLOBAL_UNLOCK();
	XSRETURN_YES;


#/*****************************************************************************
# * _DBE_dump_var()
# *****************************************************************************/

void
_DBE_dump_var( ... )
PREINIT:
	size_t rlen;
	char *str, *name;
PPCODE:
	if( items < 2 || items > 3 )
		Perl_croak( aTHX_ "usage: DBE->dump_var($var,$name=0)" );
	name = items > 2 && SvOK( ST(2) ) ? SvPV_nolen( ST(2) ) : NULL;
	str = my_dump_var( ST(1), name, &rlen, 0 );
	ST(0) = sv_2mortal( newSVpvn( str, rlen ) );
	Safefree( str );
	XSRETURN(1);


#/*****************************************************************************
# * _DBE_show_var()
# *****************************************************************************/

void
_DBE_show_var( ... )
PREINIT:
	size_t rlen;
	char *str;
PPCODE:
	if( items != 2 )
		Perl_croak( aTHX_ "usage: DBE->show_var($var)" );
	str = my_dump_var( ST(1), NULL, &rlen, 1 );
	ST(0) = sv_2mortal( newSVpvn( str, rlen ) );
	Safefree( str );
	XSRETURN(1);


#/*############################ CONNECTION CLASS #############################*/


MODULE = DBE		PACKAGE = DBE::CON			PREFIX = CON_


#/*****************************************************************************
# * CON_DESTROY()
# *****************************************************************************/

void
CON_DESTROY( ... )
PREINIT:
	dbe_con_t *con;
CODE:
	if( items < 1 || (con = dbe_con_find( ST(0) )) == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( "DESTROY called for con %lu, ref %d\n", con->id, con->refcnt );
#endif
	con->refcnt --;
	if( con->refcnt < 0 ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- DESTROY= %s=SCALAR(0x%x)",
				con->drv->class_con, SvRV( ST(0) )
			);
		dbe_con_rem( con );
	}


#/*****************************************************************************
# * CON_close( this )
# *****************************************************************************/

void
CON_close( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
CODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- DESTROY= %s=SCALAR(0x%x)",
			con->drv->class_con, SvRV( this )
		);
	dbe_con_rem( con );
	XSRETURN_YES;


#/*****************************************************************************
# * CON_reconnect( this )
# *****************************************************************************/

void
CON_reconnect( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	int r;
	unsigned int recon;
CODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	recon = con->reconnect_count;
	con->reconnect_count = 1;
	r = dbe_con_reconnect( this, "con", con );
	con->reconnect_count = recon;
	if( r != DBE_OK )
		XSRETURN_EMPTY;
	XSRETURN_YES;


#/*****************************************************************************
# * CON_ping( this )
# *****************************************************************************/

void
CON_ping( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
CODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_ping == NULL )
		NOFUNCTION_XS( this, "con", con, "ping" );
_retry:
	r = dd->con_ping( con->drv_data );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE, "<- ping()= TRUE" );
		XSRETURN_YES;
	case DBE_CONN_LOST:
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE, "<- ping()= FALSE" );
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		HANDLE_ERROR_XS( this, "con", con, "Ping failed" );		
	}


#/*****************************************************************************
# * CON_query( this, query )
# *****************************************************************************/

void
CON_query( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	dbe_res_t *res = NULL;
	char *sql, *sql3, *sql2 = NULL, **params, *key;
	STRLEN sql_len;
	int i, r, param_count, key_len;
	drv_stmt_t *stmt;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, sql + 1, sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		param_count = 0;
		params = NULL;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( param_count || items > 2 ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 2 && SvROK( ST(2) ) ) {
			sv = SvRV( ST(2) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 2; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, stmt, dd, i - 1, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( stmt, &res );
		if( dd->stmt_free != NULL )
			dd->stmt_free( stmt );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		if( res == NULL ) {
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE, "<- do('%s')= TRUE", sql );
			XPUSHs( sv_2mortal( newSViv( 1 ) ) );
			goto _cleanup;
		}
		dbe_res_add( con, res );
		sv = sv_2mortal( newSViv( (IV) res->id ) );
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- query('%s')= %s=SCALAR(0x%lx)",
				sql, con->drv->class_res, (size_t) sv
			);
		hv = gv_stashpv( con->drv->class_res, TRUE );
		XPUSHs( sv_2mortal( sv_bless( newRV( sv ), hv ) ) );
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	}
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- query('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "Query failed" );		
	}


#/*****************************************************************************
# * CON_prepare( this, query )
# *****************************************************************************/

void
CON_prepare( this, query )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	dbe_stmt_t *stmt;
	const char *sql, *sql3;
	char *sql2 = NULL, **params;
	STRLEN sql_len;
	int i, r, param_count;
	drv_stmt_t *p = NULL;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_prepare == NULL )
		NOFUNCTION_XS( this, "con", con, "prepare" );
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	r = dd->con_prepare( con->drv_data, sql3, sql_len, &p );
	switch( r ) {
	case DBE_OK:
		if( p == NULL ) {
			dbe_drv_set_error( con,
				-1, "Driver did not return a statement object" );
			goto _error;
		}
		Newxz( stmt, 1, dbe_stmt_t );
		stmt->drv_data = p;
		stmt->con = con;
		if( param_count && params != NULL ) {
			Newxz( stmt->params, param_count, dbe_param_t );
			for( i = 0; i < param_count; i ++ ) {
				stmt->params[i].name = params[i];
			}
			stmt->num_params = param_count;
			stmt->param_names = params;
		}
		dbe_stmt_add( con, stmt );
		hv = gv_stashpv( con->drv->class_stmt, FALSE );
		sv = sv_2mortal( newSViv( (IV) stmt->id ) );
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- prepare('%s')= %s=SCALAR(0x%lx)",
				sql, con->drv->class_stmt, (size_t) sv
			);
		Safefree( sql2 );
		ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
		XSRETURN(1);
	case DBE_CONN_LOST:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	}
_error:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- prepare('%s')= FALSE", sql );
	HANDLE_ERROR_XS( this, "con", con, "Prepare failed" );		


#/*****************************************************************************
# * CON_selectrow_array( this, query )
# *****************************************************************************/

void
CON_selectrow_array( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *sql, *sql2 = NULL, *sql3, **params, *key;
	STRLEN sql_len;
	drv_stmt_t *drv_stmt = NULL;
	dbe_res_t *res = NULL;
	int i, r, param_count, retcount = 0, key_len;
	HV *hv;
	SV *sv;
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( items > 2 || param_count ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 2 && SvROK( ST(2) ) ) {
			sv = SvRV( ST(2) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 2; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, i - 1, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_array('%s')= NULL", sql );
		goto _cleanup;
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- selectrow_array('%s')= TRUE (columns: %u)",
			sql, res->num_fields
		);
	if( res->type == RES_TYPE_DRV ) {
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "con", con, "fetch_row" );
		if( dbe_res_fetch_row( res, FALSE ) != DBE_OK )
			goto _cleanup;
		for( i = 0; i < (int) res->num_fields; i ++ ) {
			if( res->sv_buffer[i] != NULL )
				ST(i) = sv_2mortal( res->sv_buffer[i] );
			else
				ST(i) = sv_newmortal();
		}
		retcount = i;
	}
	else { /* RES_TYPE_VRT */
		vres = res->vrt_res, vrow = vres->rows;
		if( ! vres->row_count ) {
			goto _cleanup;
		}
		for( i = 0; i < (int) vres->column_count; i ++ ) {
			ST(i) = sv_2mortal( dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] ) );
		}
		retcount = i;
	}
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_array('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "Select row array" );
	}
	XSRETURN(retcount);


#/*****************************************************************************
# * CON_selectrow_arrayref( this, query )
# *****************************************************************************/

void
CON_selectrow_arrayref( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *sql, *sql2 = NULL, *sql3, **params, *key;
	STRLEN sql_len;
	drv_stmt_t *drv_stmt = NULL;
	dbe_res_t *res = NULL;
	AV *av;
	HV *hv;
	SV *sv;
	int i, r, param_count, key_len;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( items > 2 || param_count ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 2 && SvROK( ST(2) ) ) {
			sv = SvRV( ST(2) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 2; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, i - 1, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_arrayref('%s')= NULL", sql );
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- selectrow_arrayref('%s')= TRUE (columns: %u)",
			sql, res->num_fields
		);
	if( res->type == RES_TYPE_DRV ) {
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "con", con, "fetch_row" );
		if( dbe_res_fetch_row( res, FALSE ) != DBE_OK ) {
			ST(0) = sv_newmortal();
			goto _cleanup;
		}
		av = newAV();
		for( i = 0; i < (int) res->num_fields; i ++ ) {
			if( res->sv_buffer[i] != NULL )
				av_push( av, res->sv_buffer[i] );
			else
				av_push( av, newSV(0) );
		}
		ST(0) = sv_2mortal( newRV( sv_2mortal( (SV *) av ) ) );
	}
	else { /* RES_TYPE_VRT */
		dbe_vrt_res_t *vres = res->vrt_res;
		dbe_vrt_row_t *vrow = vres->rows;
		if( ! vres->row_count ) {
			ST(0) = sv_newmortal();
			goto _cleanup;
		}
		av = newAV();
		for( i = 0; i < (int) vres->column_count; i ++ ) {
			av_push( av, dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] ) );
		}
		ST(0) = sv_2mortal( newRV( sv_2mortal( (SV *) av ) ) );
	}
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_arrayref('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "Select row array ref" );
	}
	XSRETURN(1);


#/*****************************************************************************
# * CON_selectrow_hashref( this, query )
# *****************************************************************************/

void
CON_selectrow_hashref( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	char *sql, *sql2 = NULL, *sql3, **params, *key;
	STRLEN sql_len;
	drv_stmt_t *drv_stmt = NULL;
	HV *hv = NULL;
	SV *sv;
	int i, r, param_count, key_len;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( items > 2 || param_count ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 2 && SvROK( ST(2) ) ) {
			sv = SvRV( ST(2) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 2; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, i - 1, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_hashref('%s')= NULL", sql );
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- selectrow_hashref('%s')= TRUE (columns: %u)",
			sql, res->num_fields
		);
	if( res->type == RES_TYPE_DRV ) {
		if( dbe_res_fetch_hash( this, res, &hv, 0 ) == DBE_OK )
			ST(0) = newRV_noinc( (SV *) hv );
		else
			ST(0) = sv_newmortal();
	}
	else {
		if( dbe_vrt_res_fetch_hash( res, &hv, 0 ) == DBE_OK )
			ST(0) = newRV_noinc( (SV *) hv );
		else
			ST(0) = sv_newmortal();
	}
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectrow_hashref('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "Select row hash ref" );
	}
	XSRETURN(1);


#/*****************************************************************************
# * CON_selectall_arrayref( this, query )
# *****************************************************************************/

void
CON_selectall_arrayref( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	char *sql, *sql2 = NULL, *sql3, **params, *key;
	STRLEN sql_len;
	drv_stmt_t *drv_stmt = NULL;
	AV *av = NULL;
	HV *hv;
	SV *sv;
	int i, r, fn = 0, param_count, key_len;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( items > 2 ) {
		if( SvROK( ST(2) ) && SvTYPE( SvRV( ST(2) ) ) == SVt_PVHV )
			fn = 1;
	}
	if( items > 3 ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 3 && SvROK( ST(3) ) ) {
			sv = SvRV( ST(3) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 3; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, i - 2, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectall_arrayref('%s')= NULL", sql );
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res->type == RES_TYPE_DRV ) {
		if( dbe_res_fetchall_arrayref( this, res, &av, fn ) == DBE_OK ) {
			ST(0) = sv_2mortal( newRV( (SV *) av ) );
		}
		else {
			ST(0) = sv_newmortal();
			r = DBE_ERROR;
			goto _cleanup;
		}
	}
	else {
		if( dbe_vrt_res_fetchall_arrayref( res, &av, fn ) == DBE_OK ) {
			ST(0) = sv_2mortal( newRV( (SV *) av ) );
		}
		else {
			ST(0) = sv_newmortal();
			r = DBE_ERROR;
			goto _cleanup;
		}
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- selectall_arrayref('%s')= TRUE (columns: %u, rows: %d)",
			sql, res->num_fields, (av != NULL) ? (av_len( av ) + 1) : 0
		);
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectall_arrayref('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "selectall_arrayref failed" );
	}
	XSRETURN(1);


#/*****************************************************************************
# * CON_selectall_hashref( this, query, key )
# *****************************************************************************/

void
CON_selectall_hashref( this, query, keyfield, ... )
	SV *this;
	SV *query;
	SV *keyfield;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	dbe_str_t *fields = NULL;
	char *sql, *sql2 = NULL, *sql3, **params, *key;
	STRLEN sql_len;
	drv_stmt_t *drv_stmt = NULL;
	HV *hv = NULL;
	AV *av;
	SV **psv, *sv;
	int i, ks, r, param_count, key_len;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( SvROK( keyfield ) && (keyfield = SvRV( keyfield )) ) {
		if( SvTYPE( keyfield ) != SVt_PVAV ) {
			dbe_drv_set_error( con, -1,
				"Type of key fields must be an arrayref" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		av = (AV *) keyfield;
		ks = av_len( av ) + 1;
		if( ks <= 0 ) {
			dbe_drv_set_error( con, -1, "List of key fields is empty" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		Newx( fields, ks, dbe_str_t );
		for( i = 0; i < ks; i ++ ) {
			psv = av_fetch( av, i, 0 );
			if( psv != NULL )
				fields[i].value = SvPV( (*psv), fields[i].length );
			else {
				fields[i].value = "";
				fields[i].length = 0;
			}
		}
	}
	else {
		ks = 1;
		Newx( fields, ks, dbe_str_t );
		fields[0].value = SvPV( keyfield, fields[0].length );
	}
	if( items > 3 || param_count ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > 3 && SvROK( ST(3) ) ) {
			sv = SvRV( ST(3) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = 3; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, i - 2, ST(i), 0, NULL );
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _cleanup;
			}
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectall_hashref('%s')= TRUE", sql );
		ST(0) = sv_newmortal();
		goto _cleanup;
	}
	if( res->type == RES_TYPE_DRV ) {
		if( dbe_res_fetchall_hashref( this, res, fields, ks, &hv ) == DBE_OK )
			ST(0) = sv_2mortal( newRV( (SV *) hv ) );
		else {
			ST(0) = sv_newmortal();
			r = DBE_ERROR;
			goto _cleanup;
		}
	}
	else {
		if( dbe_vrt_res_fetchall_hashref( res, fields, ks, &hv ) == DBE_OK )
			ST(0) = sv_2mortal( newRV( (SV *) hv ) );
		else {
			ST(0) = sv_newmortal();
			r = DBE_ERROR;
			goto _cleanup;
		}
	}
	IF_TRACE_CON( con, DBE_TRACE_SQL )
		TRACE( con, DBE_TRACE_SQL, TRUE,
			"<- selectall_hashref('%s')= TRUE (keys: %d, columns: %u)",
			sql, ks, res->num_fields
		);
_cleanup:
	Safefree( fields );
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectall_hashref('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "selectall_hashref failed" );
	}
	XSRETURN(1);


#/*****************************************************************************
# * CON_select_col( this, query )
# *****************************************************************************/

void
CON_select_col( this, query, ... )
	SV *this;
	SV *query;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	char *sql, *sql2 = NULL, *sql3, *skey, **params;
	STRLEN sql_len, lkey;
	u_long i, j;
	drv_stmt_t *drv_stmt = NULL;
	int r, itemp, type, param_count;
	SV *store = NULL, *sv, **psv;
	AV *av = NULL;
	HV *hv = NULL;
	long col1 = 0, col2 = 1;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	/* select_col( <sql>, [store, col1, col2], @bind_values ) */
	itemp = 2;
	if( items > 2 ) {
		if( SvROK(ST(2)) && (sv = SvRV(ST(2))) && SvTYPE(sv) == SVt_PVAV ) {
			itemp = 3;
			av = (AV *) sv;
			psv = av_fetch( av, 0, FALSE );
			if( psv != NULL ) {
				if( SvROK( *psv ) ) {
					store = SvRV( *psv );
				}
				else {
					col1 = (long) SvIV( *psv );
				}
			}
			psv = av_fetch( av, 1, FALSE );
			if( psv != NULL ) {
				if( store != NULL ) {
					col1 = (long) SvIV( *psv );
					psv = av_fetch( av, 2, FALSE );
					if( psv != NULL ) {
						col2 = (long) SvIV( *psv );
					}
				}
				else {
					col2 = (long) SvIV( *psv );
				}
			}
			av = NULL;
			if( store != NULL ) {
				if( SvTYPE(store) == SVt_PVAV ) {
					av = (AV *) store;
				}
				else if( SvTYPE(store) == SVt_PVHV ) {
					hv = (HV *) store;
				}
			}
		}
	}
	dd = con->drv->def;
	sql = SvPV( query, sql_len );
	for( ; isSPACE( *sql ); sql ++, sql_len -- );
	if( *sql == '%' ) {
		sql3 = sql2 = dbe_convert_query(
			con, &sql[1], sql_len - 1, &sql_len, &params, &param_count );
	}
	else {
		sql3 = sql;
		params = NULL;
		param_count = 0;
	}
#ifdef DBE_DEBUG
	_debug( "SQL: %s\n", sql );
#endif
_retry:
	if( items > itemp || param_count ) {
		if( dd->con_prepare == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "prepare" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		if( dd->stmt_execute == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "execute" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_prepare( con->drv_data, sql3, sql_len, &drv_stmt );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			goto _reconnect;
		default:
			goto _cleanup;
		}
		if( items > itemp && SvROK( ST(itemp) ) ) {
			sv = SvRV( ST(itemp) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				char *key;
				int key_len;
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len)) != NULL ) {
					r = dbe_bind_param_str(
						this, "con", con, drv_stmt, dd,
						params, param_count, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _cleanup;
					}
				}
				goto _execute;
			}
		}
		for( i = itemp, j = 1; i < (u_long) items; i ++, j ++ ) {
			r = dbe_bind_param_num(
				this, "con", con, drv_stmt, dd, j, ST(i), 0, NULL );
			if( r != DBE_OK )
				XSRETURN_EMPTY;
		}
_execute:
		r = dd->stmt_execute( drv_stmt, &res );
	}
	else {
		if( dd->con_query == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "query" );
			r = DBE_ERROR;
			goto _cleanup;
		}
		r = dd->con_query( con->drv_data, sql3, sql_len, &res );
	}
	switch( r ) {
	case DBE_OK:
		break;
	case DBE_CONN_LOST:
_reconnect:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	default:
		goto _cleanup;
	}
	if( res == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectcol_array('%s')= NULL", sql );
		XPUSHs( sv_newmortal() );
		goto _cleanup;
	}
	if( res->type == RES_TYPE_DRV ) {
		psv = res->sv_buffer;
		if( col1 < 0 ) {
			col1 = (long) res->num_fields + col1;
			if( col1 < 0 )
				col1 = 0;
		}
		else if( (u_long) col1 >= res->num_fields ) {
			col1 = res->num_fields - 1;
		}
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "con", con, "fetch_row" );
		if( av != NULL ) {
			i = 0;
			while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j == col1 ) {
						if( psv[col1] != NULL )
							av_store( av, i, psv[col1] );
						else
							av_store( av, i, newSV(0) );
					}
					else if( psv[j] != NULL )
						sv_2mortal( psv[j] );
				}
				i ++;
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE, rows: %lu (col: %d)",
					sql, i, col1
				);
			XPUSHs( sv_2mortal( newSViv( 1 ) ) );
		}
		else if( hv != NULL ) {
			if( col2 < 0 ) {
				col2 = (long) res->num_fields + col2;
				if( col2 < 0 )
					col2 = 0;
			}
			else
			if( (u_long) col2 >= res->num_fields ) {
				col2 = res->num_fields - 1;
			}
			i = 0;
			while( dbe_res_fetch_row( res, TRUE ) == DBE_OK ) {
				if( psv[col1] != NULL ) {
					skey = SvPV( psv[col1], lkey );
					(void) hv_store(
						hv, skey, (I32) lkey,
						psv[col2] != NULL ? psv[col2] : newSV(0),
						0
					);
					sv_2mortal( psv[col1] );
				}
				else {
					(void) hv_store(
						hv, "", 0,
						psv[col2] != NULL ? psv[col2] : newSV(0),
						0
					);
				}
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j != col1 && j != col2 && psv[j] != NULL )
						sv_2mortal( psv[j] );
				}
				i ++;
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE"
						", rows: %lu (col1: %d, col2: %d)",
					sql, i, col1, col2
				);
			XPUSHs( sv_2mortal( newSViv( 1 ) ) );
		}
		else {
			i = 0;
			while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j == col1 ) {
						if( psv[col1] != NULL )
							XPUSHs( sv_2mortal( psv[col1] ) );
						else
							XPUSHs( sv_newmortal() );
					}
					else if( psv[j] != NULL )
						sv_2mortal( psv[j] );
				}
				i ++;
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE, rows: %lu (col: %d)",
					sql, i, col1
				);
		}
	}
	else { /* RES_TYPE_VRT */
		dbe_vrt_res_t *vres = res->vrt_res;
		if( col1 < 0 ) {
			col1 = vres->column_count + col1;
			if( col1 < 0 )
				col1 = 0;
		}
		else if( (u_long) col1 >= vres->column_count ) {
			col1 = vres->column_count - 1;
		}
		type = vres->columns[col1].type;
		if( av != NULL ) {
			for( i = 0; i < vres->row_count; i ++ ) {
				av_store( av, i, dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) );
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE, rows: %lu (col: %d)",
					sql, i, col1
				);
			XPUSHs( sv_2mortal( newSViv( 1 ) ) );
		}
		else if( hv != NULL ) {
			if( col2 < 0 ) {
				col2 = vres->column_count + col2;
				if( col2 < 0 )
					col2 = 0;
			}
			else if( (u_long) col2 >= vres->column_count ) {
				col2 = vres->column_count - 1;
			}
			for( i = 0; i < vres->row_count; i ++ ) {
				store = sv_2mortal( dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) );
				skey = SvPVx( store, lkey );
				(void) hv_store( hv, skey, (I32) lkey,
					dbe_vrt_res_fetch(
						type,
						vres->rows[i].data[col2],
						vres->rows[i].lengths[col2]
					), 0
				);
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE"
						", rows: %lu (col1: %d, col2: %d)",
					sql, i, col1, col2
				);
			XPUSHs( sv_2mortal( newSViv( 1 ) ) );
		}
		else {
			for( i = 0; i < vres->row_count; i ++ ) {
				XPUSHs( sv_2mortal( dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) ) );
			}
			IF_TRACE_CON( con, DBE_TRACE_SQL )
				TRACE( con, DBE_TRACE_SQL, TRUE,
					"<- selectcol_array('%s')= TRUE, rows: %lu (col: %d)",
					sql, i, col1
				);
		}
	}
_cleanup:
	Safefree( sql2 );
	if( params != NULL ) {
		for( i = 0; i < (u_long) param_count; i ++ )
			Safefree( params[i] );
		Safefree( params );
	}
	if( res != NULL )
		dbe_res_free( res );
	if( drv_stmt != NULL && dd->stmt_free != NULL )
		dd->stmt_free( drv_stmt );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_SQL )
			TRACE( con, DBE_TRACE_SQL, TRUE,
				"<- selectcol_array('%s')= FALSE", sql );
		HANDLE_ERROR_XS( this, "con", con, "Select failed" );
	}


#/*****************************************************************************
# * CON_insert_id( this [, catalog [, schema [, table [, field]]]] )
# *****************************************************************************/

void
CON_insert_id( this, catalog = NULL, schema = NULL, table = NULL, field = NULL )
	SV *this;
	SV *catalog;
	SV *schema;
	SV *table;
	SV *field;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	const char *cat_str, *schem_str, *tab_str, *field_str;
	STRLEN cat_len, schem_len, tab_len, field_len;
	u_xlong id;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_insert_id == NULL )
		NOFUNCTION_XS( this, "con", con, "insert_id" );
	dbe_drv_set_error( con, 0, NULL );
	if( catalog != NULL && SvPOK( catalog ) )
		cat_str = SvPV( catalog, cat_len );
	else
		cat_str = NULL, cat_len = 0;
	if( schema != NULL && SvPOK( schema ) )
		schem_str = SvPV( schema, schem_len );
	else
		schem_str = NULL, schem_len = 0;
	if( table != NULL && SvPOK( table ) )
		tab_str = SvPV( table, tab_len );
	else
		tab_str = NULL, tab_len = 0;
	if( field != NULL && SvPOK( field ) )
		field_str = SvPV( field, field_len );
	else
		field_str = NULL, field_len = 0;
_retry:
	r = dd->con_insert_id( con->drv_data, cat_str, cat_len,
			schem_str, schem_len, tab_str, tab_len, field_str, field_len, &id );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- insert_id('%s','%s','%s','%s')= %lu",
				cat_str, schem_str, tab_str, field_str, id
			);
#if UVSIZE > 4
		ST(0) = sv_2mortal( newSVuv( id ) );
#else
		if( r <= 0xffffffff ) {
			ST(0) = sv_2mortal( newSVuv( (UV) id ) );
		}
		else {
			char tmp[22], *s1;
			s1 = my_ultoa( tmp, id, 10 );
			ST(0) = sv_2mortal( newSVpvn( tmp, s1 - tmp ) );
		}
#endif
		XSRETURN(1);
		break;
	case DBE_CONN_LOST:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	case DBE_ERROR:
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- insert_id('%s','%s','%s','%s')= FALSE",
				cat_str, schem_str, tab_str, field_str
			);
		HANDLE_ERROR_XS( this, "con", con, "Get insert id failed" );
	}


#/*****************************************************************************
# * CON_affected_rows( this )
# *****************************************************************************/

void
CON_affected_rows( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	xlong r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_affected_rows == NULL )
		NOFUNCTION_XS( this, "con", con, "affected_rows" );
	r = dd->con_affected_rows( con->drv_data );
#if IVSIZE >= 8
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- affected_rows= %ld", r );
	XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
#else
	if( r >= -2147483647 && r <= 2147483647 ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- affected_rows= %ld", (size_t) r );
		XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
	}
	else {
		char tmp[22], *s1;
		s1 = my_ltoa( tmp, r, 10 );
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- affected_rows= %s", tmp );
		XPUSHs( sv_2mortal( newSVpvn( tmp, s1 - tmp ) ) );
	}
#endif


#/*****************************************************************************
# * CON_quote( this, str )
# *****************************************************************************/

void
CON_quote( this, str )
	SV *this;
	SV *str;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *val;
	STRLEN val_len;
	SV *r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	val = SvPV( str, val_len );
	dd = con->drv->def;
	if( dd->con_quote != NULL )
		r = dd->con_quote( con->drv_data, val, val_len );
	else
		r = dbe_quote( val, val_len );
	if( r == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- quote('%s')= UNDEF", val );
		XSRETURN_EMPTY;
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- quote('%s')= %s", val, SvPV_nolen( r ) );
	ST(0) = sv_2mortal( r );
	XSRETURN( 1 );


#/*****************************************************************************
# * CON_quote_bin( this, bin )
# *****************************************************************************/

void
CON_quote_bin( this, bin )
	SV *this;
	SV *bin;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *val;
	STRLEN val_len;
	SV *r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_quote_bin == NULL )
		NOFUNCTION_XS( this, "con", con, "quote_bin" );
	val = SvPV( bin, val_len );
	r = dd->con_quote_bin( con->drv_data, val, val_len );
	if( r == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE, "<- quote_bin= NULL" );
		XSRETURN_EMPTY;
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- quote_bin= %s", SvPV_nolen( r ) );
	ST(0) = sv_2mortal( r );
	XSRETURN( 1 );


#/*****************************************************************************
# * CON_quote_id( this, arg1 )
# *****************************************************************************/

void
CON_quote_id( this, arg1, ... )
	SV *this;
	SV *arg1;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	SV *r;
	char **args;
	int argc, i;
	STRLEN l;
PPCODE:
	if( arg1 != NULL ) {} /* avoid compiler warning */
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	argc = items - 1;
	Newx( args, argc, char * );
	for( i = 0; i < argc; i ++ )
		args[i] = SvPV( ST(i + 1), l );
	dd = con->drv->def;
	if( dd->con_quote_id != NULL )
		r = dd->con_quote_id( con->drv_data, (const char **) args, argc );
	else
		r = dbe_quote_id(
			(const char **) args, argc,
			con->quote_id_prefix, con->quote_id_suffix,
			con->catalog_separator, con->catalog_location
		);
	Safefree( args );
	if( r == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- quote_id= UNDEF" );
		XSRETURN_EMPTY;
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- quote_id= %s", SvPV_nolen( r ) );
	ST(0) = sv_2mortal( r );
	XSRETURN( 1 );


#/*****************************************************************************
# * CON_escape_pattern( this, str )
# *****************************************************************************/

void
CON_escape_pattern( this, str )
	SV *this;
	SV *str;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *val;
	STRLEN val_len;
	SV *r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	val = SvPVx( str, val_len );
	r = dbe_escape_pattern( val, val_len, con->escape_pattern );
	if( r == NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- escape_pattern('%s')= UNDEF", val );
		XSRETURN_EMPTY;
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- escape_pattern('%s')= %s", val, SvPV_nolen( r ) );
	ST(0) = sv_2mortal( r );
	XSRETURN( 1 );


#/*****************************************************************************
# * CON_name_convert( this [, convert] )
# *****************************************************************************/

void
CON_name_convert( this, ... )
	SV *this;
PREINIT:
	dbe_con_t *con;
	const char *convert;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( con->flags & CON_NAME_LC )
		ST(0) = sv_2mortal( newSVpvn( "lc", 2 ) );
	else if( con->flags & CON_NAME_UC )
		ST(0) = sv_2mortal( newSVpvn( "uc", 2 ) );
	else
		ST(0) = sv_2mortal( newSVpvn( "", 0 ) );
	if( items > 1 ) {
		convert = SvPV_nolen( ST(1) );
		if( my_stristr( convert, "LC" ) != NULL ) {
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- name_convert= lowercase" );
			con->flags &= (~CON_NAME_UC);
			con->flags |= CON_NAME_LC;
		}
		else if( my_stristr( convert, "UC" ) != NULL ) {
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- name_convert= uppercase" );
			con->flags &= (~CON_NAME_LC);
			con->flags |= CON_NAME_UC;
		}
		else {
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- name_convert= disabled" );
			con->flags &= (~(CON_NAME_LC + CON_NAME_UC));
		}
	}
	XSRETURN(1);


#/*****************************************************************************
# * CON_auto_commit( this [, mode] )
# *****************************************************************************/

void
CON_auto_commit( this, mode = 1 )
	SV *this;
	int mode;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_auto_commit == NULL )
		NOFUNCTION_XS( this, "con", con, "auto_commit" );
_retry:
	r = dd->con_auto_commit( con->drv_data, mode );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE,
				"<- auto_commit(%d)= TRUE", mode );
		XSRETURN_YES;
	case DBE_CONN_LOST:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	case DBE_ERROR:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE,
				"<- auto_commit(%d)= FALSE", mode );
		HANDLE_ERROR_XS( this, "con", con, "Set auto_commit failed" );
	}


#/*****************************************************************************
# * CON_begin_work( this )
# *****************************************************************************/

void
CON_begin_work( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_begin_work == NULL )
		NOFUNCTION_XS( this, "con", con, "begin_work" );
_retry1:
	r = dd->con_begin_work( con->drv_data );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- begin_work= TRUE" );
		XSRETURN_YES;
	case DBE_CONN_LOST:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry1;
	case DBE_ERROR:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- begin_work= FALSE" );
		HANDLE_ERROR_XS( this, "con", con, "Begin work failed" );		
	}


#/*****************************************************************************
# * CON_commit( this )
# *****************************************************************************/

void
CON_commit( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_commit == NULL )
		NOFUNCTION_XS( this, "con", con, "commit" );
	r = dd->con_commit( con->drv_data );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- commit= TRUE" );
		XSRETURN_YES;
	case DBE_CONN_LOST:
		/* transaction is lost! */
		dbe_con_reconnect( this, "con", con );
	case DBE_ERROR:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- commit= FALSE" );
		HANDLE_ERROR_XS( this, "con", con, "Commit failed" );		
	}


#/*****************************************************************************
# * CON_rollback( this )
# *****************************************************************************/

void
CON_rollback( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_rollback == NULL )
		NOFUNCTION_XS( this, "con", con, "rollback" );
	r = dd->con_rollback( con->drv_data );
	switch( r ) {
	case DBE_OK:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- rollback= TRUE" );
		XSRETURN_YES;
	case DBE_CONN_LOST:
		/* rollback should appear, no problem */
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			XSRETURN_YES;
	case DBE_ERROR:
		IF_TRACE_CON( con, DBE_TRACE_SQLFULL )
			TRACE( con, DBE_TRACE_SQLFULL, TRUE, "<- rollback= FALSE" );
		HANDLE_ERROR_XS( this, "con", con, "Rollback failed" );		
	}


#/*****************************************************************************
# * CON_getinfo( this, id )
# *****************************************************************************/

void
CON_getinfo( this, id )
	SV *this;
	int id;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int max, len, r, trace = 0;
	char *res, tmp[128], *tmpe = NULL;
	const char *name = NULL;
	enum dbe_getinfo_type type;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_getinfo == NULL )
		NOFUNCTION_XS( this, "con", con, "getinfo" );
	max = 256;
	Newx( res, max, char );
	IF_TRACE_CON( con, DBE_TRACE_ALL ) {
		name = dbe_trace_getinfo_name( id );
		if( name != NULL )
			tmpe = tmp + sprintf( tmp, "<- getinfo(%s)= ", name );
		else
			tmpe = tmp + sprintf( tmp, "<- getinfo(%d)= ", id );
		trace = 1;
	}
_retry:
	r = dd->con_getinfo( con->drv_data, id, res, max, &len, &type );
	switch( r ) {
	case DBE_OK:
		switch( type ) {
		case DBE_GETINFO_STRING:
		default:
			if( trace ) {
				my_strcpy( tmpe, "'%s'" );
				TRACE( con, DBE_TRACE_ALL, TRUE, tmp, res );
			}
			ST(0) = sv_2mortal( newSVpvn( res, len ) );
			break;
		case DBE_GETINFO_SHORTINT:
			if( trace ) {
				my_strcpy( tmpe, "%d" );
				TRACE( con, DBE_TRACE_ALL, TRUE, tmp, *((short int *) res) );
			}
			ST(0) = sv_2mortal( newSViv( *((short int *) res) ) );
			break;
		case DBE_GETINFO_INTEGER:
			if( trace ) {
				my_strcpy( tmpe, "%d" );
				TRACE( con, DBE_TRACE_ALL, TRUE, tmp, *((int *) res) );
			}
			ST(0) = sv_2mortal( newSViv( *((int *) res) ) );
			break;
		case DBE_GETINFO_32BITMASK:
			if( trace ) {
				my_strcpy( tmpe, "%u" );
				TRACE( con, DBE_TRACE_ALL, TRUE, tmp, *((unsigned int *) res) );
			}
			ST(0) = sv_2mortal( newSVuv( *((unsigned int *) res) ) );
			break;
		}
		Safefree( res );
		XSRETURN(1);
	case DBE_GETINFO_TRUNCATE:
		max = len + 256;
		Renew( res, max, char );
		goto _retry;
	case DBE_GETINFO_INVALIDARG:
		if( trace ) {
			my_strcpy( tmpe, "FALSE" );
			TRACE( con, DBE_TRACE_ALL, TRUE, tmp );
		}
		Safefree( res );
		dbe_drv_set_error( con,
			DBE_GETINFO_INVALIDARG, "Invalid argument (%d)", id );
		HANDLE_ERROR_XS( this, "con", con, "getinfo" );
	case DBE_CONN_LOST:
		if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
			goto _retry;
	case DBE_ERROR:
	default:
		if( trace ) {
			my_strcpy( tmpe, "NULL" );
			TRACE( con, DBE_TRACE_ALL, TRUE, tmp );
		}
		Safefree( res );
		HANDLE_ERROR_XS( this, "con", con, "getinfo" );
	}
	

#/*****************************************************************************
# * CON_tables( this [, catalog [, schema [, table [, type]]]] )
# *****************************************************************************/

void
CON_tables( this, catalog = NULL, schema = NULL, table = NULL, type = NULL )
	SV *this;
	SV *catalog;
	SV *schema;
	SV *table;
	SV *type;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN yl = 0, cl = 0, sl = 0, tl = 0;
	char *ys = NULL, *cs = NULL, *ss = NULL, *ts = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_tables == NULL )
		NOFUNCTION_XS( this, "con", con, "tables" );
	if( catalog != NULL && SvPOK( catalog ) )
		cs = SvPVx( catalog, cl );
	if( schema != NULL && SvPOK( schema ) )
		ss = SvPVx( schema, sl );
	if( table != NULL && SvPOK( table ) )
		ts = SvPVx( table, tl );
	if( type != NULL && SvPOK( type ) )
		ys = SvPVx( type, yl );
_retry1:
	r = dd->con_tables(
		con->drv_data, cs, cl, ss, sl, ts, tl, ys, yl, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- tables('%s','%s','%s','%s')= NULL",
					cs, ss, ts, ys
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get tables failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- tables('%s','%s','%s','%s')= %s=SCALAR(0x%x)",
			cs, ss, ts, ys, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_columns( this [, catalog [, schema [, table [, column]]]] )
# *****************************************************************************/

void
CON_columns( this, catalog = NULL, schema = NULL, table = NULL, column = NULL )
	SV *this;
	SV *catalog;
	SV *schema;
	SV *table;
	SV *column;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN cl = 0, sl = 0, tl = 0, ol = 0;
	char *cs = NULL, *ss = NULL, *ts = NULL, *os = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_columns == NULL )
		NOFUNCTION_XS( this, "con", con, "columns" );
	if( catalog != NULL && SvPOK( catalog ) )
		cs = SvPVx( catalog, cl );
	if( schema != NULL && SvPOK( schema ) )
		ss = SvPVx( schema, sl );
	if( table != NULL && SvPOK( table ) )
		ts = SvPVx( table, tl );
	if( column != NULL && SvPOK( column ) )
		os = SvPVx( column, ol );
_retry1:
	r = dd->con_columns(
		con->drv_data, cs, cl, ss, sl, ts, tl, os, ol, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- columns('%s','%s','%s','%s')= NULL",
					cs, ss, ts, os
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get columns failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- columns('%s','%s','%s','%s')= %s=SCALAR(0x%x)",
			cs, ss, ts, os, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_primary_keys( this, catalog, schema, table )
# *****************************************************************************/

void
CON_primary_keys( this, catalog, schema, table )
	SV *this;
	SV *catalog;
	SV *schema;
	SV *table;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN cl = 0, sl = 0, tl = 0;
	char *cs = NULL, *ss = NULL, *ts = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_primary_keys == NULL )
		NOFUNCTION_XS( this, "con", con, "primary_keys" );
	ts = SvPVx( table, tl );
	if( ! tl ) {
		dbe_drv_set_error( con, -1, "No table defined" );
		XSRETURN_EMPTY;
	}
	if( schema != NULL && SvPOK( schema ) )
		ss = SvPVx( schema, sl );
	if( catalog != NULL && SvPOK( catalog ) )
		cs = SvPVx( catalog, cl );
_retry1:
	r = dd->con_primary_keys( con->drv_data, cs, cl, ss, sl, ts, tl, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- primary_keys('%s','%s','%s')= NULL",
					cs, ss, ts
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get primary keys failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- primary_keys('%s','%s','%s')= %s=SCALAR(0x%x)",
			cs, ss, ts, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_foreign_keys( this, pk_cat, pk_schem, pk_table [, pk_cat [, pk_schem [, pk_table]]] )
# *****************************************************************************/

void
CON_foreign_keys( this, pk_cat, pk_schem, pk_table, fk_cat = NULL, fk_schem = NULL, fk_table = NULL )
	SV *this;
	SV *pk_cat;
	SV *pk_schem;
	SV *pk_table;
	SV *fk_cat;
	SV *fk_schem;
	SV *fk_table;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN pcl = 0, psl = 0, ptl = 0;
	char *pcs = NULL, *pss = NULL, *pts = NULL;
	STRLEN fcl = 0, fsl = 0, ftl = 0;
	char *fcs = NULL, *fss = NULL, *fts = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_foreign_keys == NULL )
		NOFUNCTION_XS( this, "con", con, "foreign_keys" );
	if( pk_cat != NULL && SvPOK( pk_cat ) )
		pcs = SvPVx( pk_cat, pcl );
	if( pk_schem != NULL && SvPOK( pk_schem ) )
		pss = SvPVx( pk_schem, psl );
	if( pk_table != NULL && SvPOK( pk_table ) )
		pts = SvPVx( pk_table, ptl );
	if( fk_cat != NULL && SvPOK( fk_cat ) )
		fcs = SvPVx( fk_cat, fcl );
	if( fk_schem != NULL && SvPOK( fk_schem ) )
		fss = SvPVx( fk_schem, fsl );
	if( fk_table != NULL && SvPOK( fk_table ) )
		fts = SvPVx( fk_table, ftl );
	if( ! ptl && ! ftl ) {
		dbe_drv_set_error( con, -1, "At least one table name is required" );
		HANDLE_ERROR_XS( this, "con", con, "Get foreign keys" );
	}
_retry1:
	r = dd->con_foreign_keys( con->drv_data,
		pcs, pcl, pss, psl, pts, ptl, fcs, fcl, fss, fsl, fts, ftl, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- foreign_keys('%s','%s','%s','%s','%s','%s')= NULL",
					pcs, pss, pts, fcs, fss, fts
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get foreign keys" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- foreign_keys('%s','%s','%s','%s','%s','%s')= %s=SCALAR(0x%x)",
			pcs, pss, pts, fcs, fss, fts, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_data_sources( this [, wild] )
# *****************************************************************************/

void
CON_data_sources( this, wild = NULL )
	SV *this;
	SV *wild;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	char *sw = NULL;
	STRLEN lw = 0;
	int r, dc = 0;
	SV **ds = NULL;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_data_sources == NULL )
		NOFUNCTION_XS( this, "con", con, "data_sources" );
	if( wild != NULL )
		sw = SvPVx( wild, lw );
_retry1:
	r = dd->con_data_sources( con->drv_data, sw, lw, &ds, &dc );
	if( r != DBE_OK || dc <= 0 || ds == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- data_sources('%s')= FALSE", sw );
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get data sources failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- data_sources('%s')= TRUE (count: %d)", sw, dc );
	if( ds == NULL )
		XSRETURN_EMPTY;
	for( r = 0; r < dc; r ++ ) {
		if( ds[r] != NULL )
			XPUSHs( sv_2mortal( ds[r] ) );
	}
	if( dd->mem_free != NULL )
		dd->mem_free( ds );
	else
		Safefree( ds );


#/*****************************************************************************
# * CON_statistics( this [, catalog [, schema [, table [, unique_only]]]] )
# *****************************************************************************/

void
CON_statistics( this, catalog = NULL, schema = NULL, table = NULL, unique_only = 0 )
	SV *this;
	SV *catalog;
	SV *schema;
	SV *table;
	int unique_only;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN cl = 0, sl = 0, tl = 0;
	char *cs = NULL, *ss = NULL, *ts = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_statistics == NULL )
		NOFUNCTION_XS( this, "con", con, "statistics" );
	if( catalog != NULL && SvPOK( catalog ) )
		cs = SvPVx( catalog, cl );
	if( schema != NULL && SvPOK( schema ) )
		ss = SvPVx( schema, sl );
	if( table != NULL && SvPOK( table ) )
		ts = SvPVx( table, tl );
_retry1:
	r = dd->con_statistics(
		con->drv_data, cs, cl, ss, sl, ts, tl, unique_only, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- statistics('%s','%s','%s','%d')= NULL",
					cs, ss, ts, unique_only
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get statistics failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- statistics('%s','%s','%s','%d')= %s=SCALAR(0x%x)",
			cs, ss, ts, unique_only, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_special_columns( this [, catalog [, schema [, table [, unique_only]]]] )
# *****************************************************************************/

void
CON_special_columns( this, type = 1, catalog = NULL, schema = NULL, table = NULL, scope = 0, nullable = 1 )
	SV *this;
	int type;
	SV *catalog;
	SV *schema;
	SV *table;
	int scope;
	int nullable;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	STRLEN cl = 0, sl = 0, tl = 0;
	char *cs = NULL, *ss = NULL, *ts = NULL;
	int r;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	/*
	switch( type ) {
	case SQL_BEST_ROWID:
	case SQL_ROWVER:
		break;
	default:
		dbe_drv_set_error( con, -1, "Invalid identifier type %d", type );
		HANDLE_ERROR_XS( this, "con", con, "Get special columns" );
	}
	*/
	dd = con->drv->def;
	if( dd->con_special_columns == NULL )
		NOFUNCTION_XS( this, "con", con, "special_columns" );
	if( catalog != NULL && SvPOK( catalog ) )
		cs = SvPVx( catalog, cl );
	if( schema != NULL && SvPOK( schema ) )
		ss = SvPVx( schema, sl );
	if( table != NULL && SvPOK( table ) )
		ts = SvPVx( table, tl );
_retry1:
	r = dd->con_special_columns(
		con->drv_data, type, cs, cl, ss, sl, ts, tl, scope, nullable,
		&res
	);
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- special_columns('%s','%s','%s',%d,%d)= NULL",
					cs, ss, ts, scope, nullable
				);
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get special columns" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, FALSE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- special_columns('%s','%s','%s',%d,%d)= %s=SCALAR(0x%x)",
			cs, ss, ts, scope, nullable, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_type_info( this, sql_type )
# *****************************************************************************/

void
CON_type_info( this, sql_type = NULL )
	SV *this;
	SV *sql_type;
PREINIT:
	dbe_con_t *con;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	int r;
	enum sql_type type;
	SV *sv;
	HV *hv;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_type_info == NULL )
		NOFUNCTION_XS( this, "con", con, "type_info" );
	if( sql_type == NULL || ! SvOK( sql_type ) )
		type = SQL_ALL_TYPES;
	else
	if( SvIOK( sql_type ) )
		type = (enum sql_type) SvIV( sql_type );
	else
		type = dbe_sql_type_by_name( SvPV_nolen( sql_type ) );
_retry1:
	r = dd->con_type_info( con->drv_data, type, &res );
	if( r != DBE_OK || res == NULL ) {
		switch( r ) {
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry1;
		case DBE_ERROR:
		case DBE_OK:
		default:
			IF_TRACE_CON( con, DBE_TRACE_ALL )
				TRACE( con, DBE_TRACE_ALL, TRUE,
					"<- type_info('%d')= NULL", (int) type );
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "con", con, "Get type info failed" );
			dbe_drv_set_error( con, 0, NULL );	
			XSRETURN_EMPTY;
		}
	}
	dbe_res_add( con, res );
	hv = gv_stashpv( con->drv->class_res, TRUE );
	sv = sv_2mortal( newSViv( (IV) res->id ) );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- type_info('%d')= %s=SCALAR(0x%x)",
			(int) type, con->drv->class_res, (size_t) sv
		);
	ST(0) = sv_2mortal( sv_bless( newRV( sv ), hv ) );
	XSRETURN(1);


#/*****************************************************************************
# * CON_row_limit()
# *****************************************************************************/

void
CON_row_limit( this, ... )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	u_xlong limit, offset;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	if( dd->con_row_limit == NULL )
		NOFUNCTION_XS( this, "con", con, "row_limit" );
	if( items > 1 )
		limit = SvUV( ST(1) );
	else
		limit = 0;
	if( items > 2 )
		offset = SvUV( ST(2) );
	else
		offset = 0;
	r = dd->con_row_limit( con->drv_data, limit, offset );
	if( r != DBE_OK ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- row_limit('%lu','%lu')= FALSE", limit, offset );
		HANDLE_ERROR_XS( this, "con", con, "Set row limit" );
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- row_limit('%lu','%lu')= TRUE", limit, offset );
	XSRETURN_YES;


#/*****************************************************************************
# * CON_driver_has( this, name )
# *****************************************************************************/

#define CON_driver_has(fnc) { \
	f = 1; \
	x = fnc; \
	goto check; \
}

void
CON_driver_has( this, name )
	SV *this;
	const char *name;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	const void *x = NULL;
	int f = 0;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	switch( tolower( *name ) ) {
	case 'a':
		if( my_stricmp( name, "affected_rows" ) == 0 )
			CON_driver_has( dd->con_affected_rows );
		if( my_stricmp( name, "attr_get" ) == 0 )
			CON_driver_has( dd->con_get_attr );
		if( my_stricmp( name, "attr_set" ) == 0 )
			CON_driver_has( dd->con_set_attr );
		if( my_stricmp( name, "auto_commit" ) == 0 )
			CON_driver_has( dd->con_auto_commit );
		break;
	case 'b':
		if( my_stricmp( name, "begin_work" ) == 0 )
			CON_driver_has( dd->con_begin_work );
		if( my_stricmp( name, "bind_param" ) == 0 )
			CON_driver_has( dd->stmt_bind_param );
		break;
	case 'c':
		if( my_stricmp( name, "columns" ) == 0 )
			CON_driver_has( dd->con_columns );
		if( my_stricmp( name, "commit" ) == 0 )
			CON_driver_has( dd->con_commit );
		break;
	case 'd':
		if( my_stricmp( name, "data_sources" ) == 0 )
			CON_driver_has( dd->con_data_sources );
		break;
	case 'e':
		if( my_stricmp( name, "execute" ) == 0 )
			CON_driver_has( dd->stmt_execute );
		break;
	case 'f':
		if( my_stricmp( name, "fetch_field" ) == 0 )
			CON_driver_has( dd->res_fetch_field );
		if( my_stricmp( name, "fetch_names" ) == 0 )
			CON_driver_has( dd->res_fetch_names );
		if( my_stricmp( name, "fetch_row" ) == 0 )
			CON_driver_has( dd->res_fetch_row );
		if( my_stricmp( name, "field_seek" ) == 0 )
			CON_driver_has( dd->res_field_seek );
		if( my_stricmp( name, "field_tell" ) == 0 )
			CON_driver_has( dd->res_field_tell );
		if( my_stricmp( name, "foreign_keys" ) == 0 )
			CON_driver_has( dd->con_foreign_keys );
		break;
	case 'g':
		if( my_stricmp( name, "getinfo" ) == 0 )
			CON_driver_has( dd->con_getinfo );
		break;
	case 'i':
		if( my_stricmp( name, "insert_id" ) == 0 )
			CON_driver_has( dd->con_insert_id );
		break;
	case 'l':
		if( my_stricmp( name, "lob_read" ) == 0 )
			CON_driver_has( dd->con_lob_read );
		if( my_stricmp( name, "lob_size" ) == 0 )
			CON_driver_has( dd->con_lob_size );
		if( my_stricmp( name, "lob_write" ) == 0 )
			CON_driver_has( dd->con_lob_write );
		break;
	case 'p':
		if( my_stricmp( name, "param_count" ) == 0 )
			CON_driver_has( dd->stmt_param_count );
		if( my_stricmp( name, "prepare" ) == 0 )
			CON_driver_has( dd->con_prepare );
		if( my_stricmp( name, "primary_keys" ) == 0 )
			CON_driver_has( dd->con_primary_keys );
		break;
	case 'q':
		if( my_stricmp( name, "query" ) == 0 )
			CON_driver_has( dd->con_query );
		if( my_stricmp( name, "quote" ) == 0 )
			CON_driver_has( dd->con_quote );
		if( my_stricmp( name, "quote_bin" ) == 0 )
			CON_driver_has( dd->con_quote_bin );
		if( my_stricmp( name, "quote_id" ) == 0 )
			CON_driver_has( dd->con_quote_id );
		break;
	case 'r':
		if( my_stricmp( name, "reconnect" ) == 0 )
			CON_driver_has( dd->con_reconnect );
		if( my_stricmp( name, "rollback" ) == 0 )
			CON_driver_has( dd->con_rollback );
		if( my_stricmp( name, "row_limit" ) == 0 )
			CON_driver_has( dd->con_row_limit );
		if( my_stricmp( name, "row_seek" ) == 0 )
			CON_driver_has( dd->res_row_seek );
		if( my_stricmp( name, "row_tell" ) == 0 )
			CON_driver_has( dd->res_row_tell );
		break;
	case 's':
		if( my_stricmp( name, "statistics" ) == 0 )
			CON_driver_has( dd->con_statistics );
		break;
	case 't':
		if( my_stricmp( name, "tables" ) == 0 )
			CON_driver_has( dd->con_tables );
		if( my_stricmp( name, "type_info" ) == 0 )
			CON_driver_has( dd->con_type_info );
		break;
	}
check:
	if( x != NULL ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- driver_has('%s')= TRUE", name );
		XSRETURN_YES;
	}
	if( f != 0 ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- driver_has('%s')= FALSE", name
		);
		XSRETURN_NO;
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE, "<- driver_has('%s')= NULL", name );
	dbe_drv_set_error( con, -1, "Unknown function '%s'", name );
	HANDLE_ERROR_XS( this, "con", con, "Driver has" );


#/*****************************************************************************
# * CON_trace()
# *****************************************************************************/

void
CON_trace( this, ... )
	SV *this;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
PPCODE:
	/* Usage: $con->trace( [level [, handle]] ) */
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = con->drv->def;
	con->trace_local = 1;
	if( items > 1 ) {
		if( SvIOK( ST(1) ) ) {
			con->trace_level = (int) SvIV( ST(1) );
		}
		else {
			con->trace_level =
				dbe_trace_level_by_name( SvPV_nolen( ST(1) ) );
		}
	}
	else {
		con->trace_level = DBE_TRACE_NONE;
	}
	if( dd->con_set_trace != NULL )
		dd->con_set_trace( con->drv_data, con->trace_level );
	if( con->trace_level <= DBE_TRACE_NONE )
		XSRETURN_YES;
	if( items > 2 && SvOK( ST(2) ) ) {
		if( con->sv_trace_out != NULL )
			SvREFCNT_dec( con->sv_trace_out );
		con->sv_trace_out = SvREFCNT_inc( ST(2) );
	}
	TRACE( NULL, 0, TRUE,
		"DBE v%s build %u %s trace level set to %s [%d] for %s=SCALAR(0x%lx)",
		XS_VERSION, DBE_BUILD,
#ifdef USE_ITHREADS
		"thread-multi",
#else
		"",
#endif
		dbe_trace_name_by_level( con->trace_level ),
		con->trace_level, con->drv->class_con, SvRV( this )
	);
	if( ! PL_dowarn ) {
		dbe_perl_print( con->sv_trace_out,
			"    Note: Perl is running without the -w option\n", 48
		);
	}
	XSRETURN_YES;


#/*****************************************************************************
# * CON_attr_set( this, name, value )
# *****************************************************************************/

void
CON_attr_set( this, name, value )
	SV *this;
	char *name;
	SV *value;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( tolower( *name ) ) {
	case 'c':
		if( my_stricmp( name, "croak" ) == 0 ) {
			if( STR_TRUE( SvPV_nolen( value ) ) )
				con->flags |= CON_ONERRORDIE;
			else
				con->flags &= (~CON_ONERRORDIE);
		}
		else
			goto _default;
		break;
	case 'r':
		if( my_stricmp( name, "reconnect" ) == 0 ) {
			con->reconnect_count = (u_int) SvUV( value );
		}
		else
			goto _default;
		break;
	case 'w':
		if( my_stricmp( name, "warn" ) == 0 ) {
			if( STR_TRUE( SvPV_nolen( value ) ) )
				con->flags |= CON_ONERRORWARN;
			else
				con->flags &= (~CON_ONERRORWARN);
		}
		else
			goto _default;
		break;
	default:
_default:
		dd = con->drv->def;
		if( dd->con_set_attr == NULL )
			goto _invalid;
_retry:
		r = dd->con_set_attr( con->drv_data, name, value );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_INVALIDARG:
			goto _invalid;
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry;
		case DBE_ERROR:
		default:
			goto _error;
		}
	}
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- attr_set('%s' => '%s')= TRUE", name, SvPV_nolen( value ) );
	XSRETURN_YES;
_invalid:
	dbe_drv_set_error( con, -1, "Invalid attribute '%s'", name );
_error:
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- attr_set('%s' => '%s')= FALSE", name, SvPV_nolen( value ) );
	HANDLE_ERROR_XS( this, "con", con, "Set attr failed" );


#/*****************************************************************************
# * CON_attr_get( this, name )
# *****************************************************************************/

void
CON_attr_get( this, name )
	SV *this;
	char *name;
PREINIT:
	dbe_con_t *con;
	drv_def_t *dd;
	int r;
	SV *ret = NULL;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( tolower( *name ) ) {
	case 'c':
		if( my_stricmp( name, "croak" ) == 0 ) {
			if( con->flags & CON_ONERRORDIE )
				ret = newSViv( 1 );
			else
				ret = newSViv( 0 );
		}
		else
			goto _default;
		break;
	case 'r':
		if( my_stricmp( name, "reconnect" ) == 0 ) {
			ret = newSVuv( (UV) con->reconnect_count );
		}
		else
			goto _default;
		break;
	case 'w':
		if( my_stricmp( name, "warn" ) == 0 ) {
			if( con->flags & CON_ONERRORWARN )
				ret = newSViv( 1 );
			else
				ret = newSViv( 0 );
		}
		else
			goto _default;
		break;
	default:
_default:
		dd = con->drv->def;
		if( dd->con_get_attr == NULL )
			goto _invalid;
_retry:
		r = dd->con_get_attr( con->drv_data, name, &ret );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_INVALIDARG:
			goto _invalid;
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "con", con ) == DBE_OK )
				goto _retry;
		case DBE_ERROR:
		default:
			goto _error;
		}
	}
	if( ret == NULL )
		ret = newSV(0);
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- attr_get('%s')= '%s'", name, SvPV_nolen( ret ) );
	ST(0) = sv_2mortal( ret );
	XSRETURN(1);
_invalid:
	dbe_drv_set_error( con, -1, "Invalid attribute '%s'", name );
_error:
	if( ret != NULL )
		ret = sv_2mortal( ret );
	IF_TRACE_CON( con, DBE_TRACE_ALL )
		TRACE( con, DBE_TRACE_ALL, TRUE,
			"<- attr_get('%s')= FALSE", name );
	HANDLE_ERROR_XS( this, "con", con, "Get attr failed" );


#/*****************************************************************************
# * CON_set_error_handler( this, fnc, arg )
# *****************************************************************************/

void
CON_set_error_handler( this, fnc = NULL, arg = NULL )
	SV *this;
	SV *fnc;
	SV *arg;
PREINIT:
	dbe_con_t *con;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( con->error_handler != NULL ) {
		sv_2mortal( con->error_handler );
		if( con->error_handler_arg != NULL )
			sv_2mortal( con->error_handler_arg );
	}
	if( fnc == NULL || ! SvOK(fnc) || (! SvROK(fnc) && ! SvPOK(fnc)) ) {
#ifdef DBE_DEBUG
		_debug( "clear error_handler\n" );
#endif
		con->error_handler = NULL;
		con->error_handler_arg = NULL;
	}
	else {
#ifdef DBE_DEBUG
		_debug( "set error_handler %d\n", SvTYPE( fnc ) );
#endif
		con->error_handler = newSVsv( fnc );
		if( arg != NULL )
			con->error_handler_arg = newSVsv( arg );
	}
	XSRETURN_YES;


#/*****************************************************************************
# * CON_errno( this )
# *****************************************************************************/

void
CON_errno( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	XSRETURN_IV( (IV) con->last_errno );


#/*****************************************************************************
# * CON_error( this )
# *****************************************************************************/

void
CON_error( this )
	SV *this;
PREINIT:
	dbe_con_t *con;
PPCODE:
	if( (con = dbe_con_find( this )) == NULL )
		XSRETURN_EMPTY;
	ST(0) = sv_2mortal( newSVpvn(
		con->last_error, strlen( con->last_error ) ) );
	XSRETURN(1);


#/*############################# RESULT CLASS ################################*/


MODULE = DBE		PACKAGE = DBE::RES			PREFIX = RES_


#/*****************************************************************************
# * RES_DESTROY()
# *****************************************************************************/

void
RES_DESTROY( ... )
PREINIT:
	dbe_res_t *res;
CODE:
	if( items < 1 || (res = dbe_res_find( ST(0) )) == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( "DESTROY called for res %lu, ref %d\n", res->id, res->refcnt ); 
#endif
	res->refcnt --;
	if( res->refcnt < 0 ) {
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- DESTROY= %s=SCALAR(0x%x)",
				res->con->drv->class_res, SvRV( ST(0) )
			);
		dbe_res_rem( res );
	}


#/*****************************************************************************
# * RES_close( this )
# *****************************************************************************/

void
RES_close( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
CODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	IF_TRACE_CON( res->con, DBE_TRACE_ALL )
		TRACE( res->con, DBE_TRACE_ALL, TRUE,
			"<- DESTROY= %s=SCALAR(0x%x)",
			res->con->drv->class_res, SvRV( this )
		);
	dbe_res_rem( res );
	XSRETURN_YES;


#/*****************************************************************************
# * RES_bind_column( this )
# *****************************************************************************/

void
RES_bind_column( this, c_num, c_var = NULL )
	SV *this;
	unsigned long c_num;
	SV *c_var;
PREINIT:
	dbe_res_t *res;
	SV **psv;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( c_num < 1 || c_num > res->num_fields ) {
		dbe_drv_set_error( res->con, -1, "Column %lu out of range (%lu-%lu)",
			c_num, res->num_fields ? 1 : 0, res->num_fields );
		HANDLE_ERROR_XS( this, "res", res->con, "Bind column failed" );
	}
	psv = res->sv_bind + c_num - 1;
	if( *psv != NULL ) {
		/* release previous bound variable */
#if (DBE_DEBUG > 1)
		_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
		SvREFCNT_dec( *psv );
	}
	if( c_var == NULL || SvREADONLY( c_var ) ) {
		*psv = NULL;
	}
	else if( SvROK( c_var ) ) {
		*psv = SvREFCNT_inc( SvRV( c_var ) );
#if (DBE_DEBUG > 1)
		_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
	}
	else {
		*psv = SvREFCNT_inc( c_var );
#if (DBE_DEBUG > 1)
		_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
	}
	XSRETURN_YES;


#/*****************************************************************************
# * RES_bind( this )
# *****************************************************************************/

void
RES_bind( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	SV **psv, *c_var;
	u_long i;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	for( i = 1, psv = res->sv_bind; i < (u_long) items; i ++, psv ++ ) {
		if( i > res->num_fields ) {
			dbe_drv_set_error( res->con, -1,
				"Column %lu out of range (%lu-%lu)", i, 1, res->num_fields );
			HANDLE_ERROR_XS( this, "res", res->con, "Bind column failed" );
		}
		c_var = ST(i);
		if( *psv != NULL ) {
			/* release previous bound variable */
#if (DBE_DEBUG > 1)
			_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
			SvREFCNT_dec( *psv );
		}
		if( c_var == NULL || SvREADONLY( c_var ) ) {
			*psv = NULL;
		}
		else if( SvROK( c_var ) ) {
			*psv = SvREFCNT_inc( SvRV( c_var ) );
#if (DBE_DEBUG > 1)
			_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
		}
		else {
			*psv = SvREFCNT_inc( c_var );
#if (DBE_DEBUG > 1)
			_debug( "sv 0x%lx refcnt %d\n", *psv, SvREFCNT( *psv ) );
#endif
		}
	}
	XSRETURN_YES;


#/*****************************************************************************
# * RES_num_fields( this )
# *****************************************************************************/

void
RES_num_fields( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	IF_TRACE_CON( res->con, DBE_TRACE_ALL )
		TRACE( res->con, DBE_TRACE_ALL, TRUE,
			"<- num_fields= %lu", res->num_fields );
	ST(0) = sv_2mortal( newSVuv( res->num_fields ) );
	XSRETURN(1);


#/*****************************************************************************
# * RES_num_rows( this )
# *****************************************************************************/

void
RES_num_rows( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	xlong r;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_num_rows != NULL ) {
			r = dd->res_num_rows( res->drv_data );
#if IVSIZE >= 8
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- num_rows= %ld", r );
			XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
#else
			if( r >= -2147483647 && r <= 2147483647 ) {
				IF_TRACE_CON( res->con, DBE_TRACE_ALL )
					TRACE( res->con, DBE_TRACE_ALL, TRUE,
						"<- num_rows= %ld", r );
				XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
			}
			else {
				char tmp[22], *s1;
				s1 = my_ltoa( tmp, r, 10 );
				IF_TRACE_CON( res->con, DBE_TRACE_ALL )
					TRACE( res->con, DBE_TRACE_ALL, TRUE,
						"<- num_rows= %s", tmp );
				XPUSHs( sv_2mortal( newSVpvn( tmp, s1 - tmp ) ) );
			}
#endif
		}
		else
			XSRETURN_IV( -1 );
		break;
	case RES_TYPE_VRT:
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- num_rows= %lu",
				((dbe_vrt_res_t *) res->drv_data)->row_count
			);
		XPUSHs( sv_2mortal( newSVuv(
			((dbe_vrt_res_t *) res->drv_data)->row_count ) ) );
		break;
	}


#/*****************************************************************************
# * RES_fetch_names( this [, name] )
# *****************************************************************************/

void
RES_fetch_names( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	dbe_vrt_res_t *vres;
	u_long i, cn = 0;
	char *name = NULL, *p1;
	size_t name_len = 0, len;
PPCODE:
	if( items > 1 && SvPOK( ST(1) ) ) {
		name = SvPVx( ST(1), name_len );
		if( my_stristr( name, "LC" ) != NULL )
			cn = 1;
		else if( my_stristr( name, "UC" ) != NULL )
			cn = 2;
		else if( my_stristr( name, "ORG" ) != NULL )
			cn = 3;
	}
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		if( res->names == NULL ) {
			if( dbe_res_fetch_names( this, res ) != DBE_OK ) {
				IF_TRACE_CON( res->con, DBE_TRACE_ALL )
					TRACE( res->con, DBE_TRACE_ALL, TRUE,
						"<- fetch_names= FALSE" );
				XSRETURN_EMPTY;
			}
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_names= TRUE (columns: %d)", res->num_fields );
		switch( cn ) {
		case 1:
			for( i = 0; i < res->num_fields; i ++ ) {
				if( name_len < res->names[i].length * 2 ) {
					name_len = res->names[i].length * 2;
					Renew( name, name_len + 1, char );
				}
				len = my_utf8_toupper(
					res->names[i].value, res->names[i].length, name, name_len );
				if( len < 0 ) {
					p1 = my_strcpyl( name, res->names[i].value );
					len = p1 - name;
				}
				XPUSHs( sv_2mortal( newSVpvn( name, len ) ) );
			}
			break;
		case 2:
			for( i = 0; i < res->num_fields; i ++ ) {
				if( name_len < res->names[i].length * 2 ) {
					name_len = res->names[i].length * 2;
					Renew( name, name_len + 1, char );
				}
				len = my_utf8_tolower(
					res->names[i].value, res->names[i].length, name, name_len );
				if( len < 0 ) {
					p1 = my_strcpyu( name, res->names[i].value );
					len = p1 - name;
				}
				XPUSHs( sv_2mortal( newSVpvn( name, len ) ) );
			}
			break;
		case 3:
			for( i = 0; i < res->num_fields; i ++ ) {
				XPUSHs( sv_2mortal(
					newSVpvn( res->names[i].value, res->names[i].length ) ) );
			}
			break;
		default:
			for( i = 0; i < res->num_fields; i ++ ) {
				XPUSHs( sv_2mortal(
					newSVpvn( res->names_conv[i].value,
						res->names_conv[i].length ) ) );
			}
			break;
		}
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_names= TRUE (fields: %d)", vres->column_count );
		switch( cn ) {
		case 1:
			for( i = 0; i < vres->column_count; i ++ ) {
				if( name_len < vres->columns[i].name_length ) {
					name_len = vres->columns[i].name_length;
					Renew( name, name_len + 1, char );
				}
				p1 = my_strcpyl( name, vres->columns[i].name );
				XPUSHs( sv_2mortal( newSVpvn( name, p1 - name ) ) );
			}
			break;
		case 2:
			for( i = 0; i < vres->column_count; i ++ ) {
				if( name_len < vres->columns[i].name_length ) {
					name_len = vres->columns[i].name_length;
					Renew( name, name_len + 1, char );
				}
				p1 = my_strcpyu( name, vres->columns[i].name );
				XPUSHs( sv_2mortal( newSVpvn( name, p1 - name ) ) );
			}
			break;
		default:
			for( i = 0; i < vres->column_count; i ++ )
				XPUSHs( sv_2mortal( newSVpvn(
					vres->columns[i].name_conv,
						vres->columns[i].name_conv_length ) ) );
			break;
		}
		break;
	}
	Safefree( name );


#/*****************************************************************************
# * RES_fetch_field( this )
# *****************************************************************************/

void
RES_fetch_field( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	dbe_vrt_column_t *vcol;
	SV **args = NULL;
	int flags = 0, count, i;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_field == NULL )
			NOFUNCTION_XS( this, "res", res->con, "fetch_field" );
		count = dd->res_fetch_field( res->drv_data, &args, &flags );
		if( count <= 0 || args == NULL ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_field= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_field= TRUE (keys: %d)", count / 2 );
		if( (count % 2) != 0 )
			Perl_croak( aTHX_ "Odd number of elements in hash" );
		for( i = 0; i < count; i ++ ) {
			if( args[i] != NULL )
				XPUSHs( sv_2mortal( args[i] ) );
			else
				XPUSHs( sv_2mortal( newSV(0) ) );
		}
		if( (flags & DBE_FREE_ARRAY) != 0 ) {
			if( dd->mem_free != NULL )
				dd->mem_free( args );
			else
				Safefree( args );
		}
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		if( vres->column_pos >= vres->column_count ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_field= FALSE" );
			XSRETURN_EMPTY;
		}
		vcol = vres->columns + vres->column_pos ++;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_field= TRUE (keys: 4)" );
		XPUSHs( sv_2mortal( newSVpvn( "COLUMN_NAME", 11 ) ) );
		XPUSHs( sv_2mortal( newSVpvn( vcol->name, vcol->name_length ) ) );
		XPUSHs( sv_2mortal( newSVpvn( "DATA_TYPE", 9 ) ) );
		XPUSHs( sv_2mortal( newSViv( vcol->type ) ) );
		XPUSHs( sv_2mortal( newSVpvn( "TYPE_NAME", 9 ) ) );
		XPUSHs( sv_2mortal( newSVpv( DBE_TYPE_NAME( vcol->type ), 0 ) ) );
		XPUSHs( sv_2mortal( newSVpvn( "NULLABLE", 8 ) ) );
		XPUSHs( sv_2mortal( newSViv( 1 ) ) );
		break;
	}


#/*****************************************************************************
# * RES_field_seek( this, offset )
# *****************************************************************************/

void
RES_field_seek( this, offset )
	SV *this;
	unsigned long offset;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	int r;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_field_seek == NULL )
			NOFUNCTION_XS( this, "res", res->con, "field_seek" );
		r = dd->res_field_seek( res->drv_data, offset );
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- field_seek= %lu (previous: %lu)", offset, r );
		XPUSHs( sv_2mortal( newSViv( r ) ) );
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- field_seek= %lu (previous: %lu)",
				offset, vres->column_pos
			);
		XPUSHs( sv_2mortal( newSViv( vres->column_pos ) ) );
		if( offset < 0 ) {
			offset = vres->column_pos + offset;
			if( offset < 0 )
				offset = 0;
		}
		else
		if( (u_long) offset > vres->column_count ) {
			offset = vres->column_count;
		}
		vres->column_pos = offset;
		break;
	}


#/*****************************************************************************
# * RES_field_tell( this )
# *****************************************************************************/

void
RES_field_tell( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	u_long r;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_field_tell == NULL )
			NOFUNCTION_XS( this, "res", res->con, "field_tell" );
		r = dd->res_field_tell( res->drv_data );
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- field_tell= %lu", r );
		XPUSHs( sv_2mortal( newSVuv( (UV) r ) ) );
		break;
	case RES_TYPE_VRT:
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- field_tell= %lu",
				((dbe_vrt_res_t *) res->drv_data)->column_pos
			);
		XPUSHs( sv_2mortal( newSViv(
			((dbe_vrt_res_t *) res->drv_data)->column_pos ) ) );
		break;
	}


#/*****************************************************************************
# * RES_fetch( this )
# *****************************************************************************/

void
RES_fetch( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	u_long i;
	SV *sv;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "res", res->con, "fetch" );
		if( dbe_res_fetch_row( res, FALSE ) != DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch= TRUE (columns: %u, row: %lu)",
				res->num_fields,
				dd->res_row_tell ? dd->res_row_tell( res->drv_data ) + 1 : 0
			);
		for( i = 0; i < res->num_fields; i ++ ) {
			sv = res->sv_buffer[i] != NULL
				? sv_2mortal( res->sv_buffer[i] ) : sv_newmortal();
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
		}
		break;
	case RES_TYPE_VRT:
		vres = res->vrt_res;
		if( vres->row_pos >= vres->row_count ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch= FALSE" );
			XSRETURN_EMPTY;
		}
		vrow = vres->rows + vres->row_pos ++;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch= TRUE (columns: %u, row: %lu)",
				vres->column_count, vres->row_pos
			);
		for( i = 0; i < vres->column_count; i ++ ) {
			sv = sv_2mortal( dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] ) );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
		}
		break;
	}
	XSRETURN_YES;


#/*****************************************************************************
# * RES_fetch_row( this )
# *****************************************************************************/

void
RES_fetch_row( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	u_long i, ft = 0;
	AV *av = NULL;
	HV *hv = NULL;
	SV *sv;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( items > 1 && SvROK( ST(1) ) ) {
		sv = SvRV( ST(1) );
		if( SvTYPE( sv ) == SVt_PVAV ) {
			av = (AV *) sv;
			ft = 1;
		}
		else if( SvTYPE( sv ) == SVt_PVHV ) {
			hv = (HV *) sv;
			ft = 2;
		}
	}
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "res", res->con, "fetch_row" );
		if( dbe_res_fetch_row( res, ft == 2 ) != DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_row= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_row= TRUE (columns: %u, row: %lu)",
				res->num_fields,
				dd->res_row_tell ? dd->res_row_tell( res->drv_data ) : 0
			);
		switch( ft ) {
		case 1:
			av_clear( av );
			for( i = 0; i < res->num_fields; i ++ ) {
				sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
				if( res->sv_bind[i] != NULL )
					sv_setsv( res->sv_bind[i], sv );
				av_store( av, i, sv );
			}
			XSRETURN_YES;
		case 2:
			hv_clear( hv );
			if( res->names == NULL )
				if( dbe_res_fetch_names( this, res ) != DBE_OK )
					XSRETURN_EMPTY;
			for( i = 0; i < res->num_fields; i ++ ) {
				sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
				if( res->sv_bind[i] != NULL )
					sv_setsv( res->sv_bind[i], sv );
				(void) hv_store( hv,
					res->names_conv[i].value, (I32) res->names_conv[i].length,
					sv, 0
				);
			}
			XSRETURN_YES;
		}
		for( i = 0; i < res->num_fields; i ++ ) {
			sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			XPUSHs( sv_2mortal( sv ) );
		}
		break;
	case RES_TYPE_VRT:
		vres = res->vrt_res;
		if( vres->row_pos >= vres->row_count ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_row= FALSE" );
			XSRETURN_EMPTY;
		}
		vrow = vres->rows + vres->row_pos ++;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetch_row= TRUE (columns: %u, row: %lu)",
				vres->column_count, vres->row_pos
			);
		switch( ft ) {
		case 1:
			av_clear( av );
			for( i = 0; i < vres->column_count; i ++ ) {
				sv = dbe_vrt_res_fetch(
					vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
				if( res->sv_bind[i] != NULL )
					sv_setsv( res->sv_bind[i], sv );
				av_store( av, i, sv );
			}
			XSRETURN_YES;
		case 2:
			hv_clear( hv );
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
			XSRETURN_YES;
		}
		for( i = 0; i < vres->column_count; i ++ ) {
			sv = dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			XPUSHs( sv_2mortal( sv ) );
		}
		break;
	}


#/*****************************************************************************
# * RES_fetch_col( this [, store [, col1 [, col2]]] )
# *****************************************************************************/

void
RES_fetch_col( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	u_long i, j;
	int type;
	SV **buffer, *store;
	AV *av = NULL;
	HV *hv = NULL;
	char *skey;
	STRLEN lkey;
	long col1 = 0, col2 = 1;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( items ) {
	case 1:
		store = NULL;
		break;
	case 4:
		col2 = (long) SvIV( ST(3) );
	case 3:
		col1 = (long) SvIV( ST(2) );
	case 2:
		store = ST(1);
		if( store != NULL && SvROK( store ) && (store = SvRV( store )) ) {
			if( SvTYPE( store ) == SVt_PVAV )
				av = (AV *) store;
			else
			if( SvTYPE( store ) == SVt_PVHV )
				hv = (HV *) store;
		}
		else {
			col1 = (long) SvIV( ST(1) );
		}
		break;
	default:
		Perl_croak( aTHX_ "Usage: $res->fetch_col(store=0,col1=0,col2=0)" );
	}
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "res", res->con, "fetch_row" );
		if( col1 < 0 ) {
			col1 = (long) res->num_fields + col1;
			if( col1 < 0 )
				col1 = 0;
		}
		else
		if( (u_long) col1 >= res->num_fields ) {
			col1 = res->num_fields - 1;
		}
		buffer = res->sv_buffer;
		if( av != NULL ) {
			i = 0;
			while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j == col1 ) {
						if( buffer[col1] != NULL )
							av_store( av, i, buffer[col1] );
						else
							av_store( av, i, newSV(0) );
					}
					else if( buffer[j] != NULL )
						sv_2mortal( buffer[j] );
				}
				i ++;
			}
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= col: %d (rows: %lu)", col1, i );
			XSRETURN_YES;
		}
		else if( hv != NULL ) {
			if( col2 < 0 ) {
				col2 = (long) res->num_fields + col2;
				if( col2 < 0 )
					col2 = 0;
			}
			else
			if( (u_long) col2 >= res->num_fields ) {
				col2 = res->num_fields - 1;
			}
			i = 0;
			while( dbe_res_fetch_row( res, TRUE ) == DBE_OK ) {
				if( buffer[col1] != NULL ) {
					skey = SvPV( buffer[col1], lkey );
					(void) hv_store(
						hv, skey, (I32) lkey,
						buffer[col2] != NULL ? buffer[col2] : newSV(0),
						0
					);
					sv_2mortal( buffer[col1] );
				}
				else {
					(void) hv_store(
						hv, "", 0,
						buffer[col2] != NULL ? buffer[col2] : newSV(0),
						0
					);
				}
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j != col1 && j != col2 && buffer[j] != NULL )
						sv_2mortal( buffer[j] );
				}
				i ++;
			}
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= col1: %d, col2: %d (rows: %lu)",
					col1, col2, i
				);
			XSRETURN_YES;
		}
		else {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= col: %d", col1 );
			while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
				for( j = 0; j < res->num_fields; j ++ ) {
					if( j == col1 ) {
						if( buffer[col1] != NULL )
							XPUSHs( sv_2mortal( buffer[col1] ) );
						else
							XPUSHs( sv_2mortal( newSV(0) ) );
					}
					else if( buffer[j] != NULL )
						sv_2mortal( buffer[j] );
				}
			}
		}
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		if( col1 < 0 ) {
			col1 = vres->column_count + col1;
			if( col1 < 0 )
				col1 = 0;
		}
		else if( (u_long) col1 >= vres->column_count ) {
			col1 = vres->column_count - 1;
		}
		type = vres->columns[col1].type;
		if( av != NULL ) {
			for( i = vres->row_pos; i < vres->row_count; i ++ ) {
				av_store( av, i, dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) );
			}
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= col: %d (rows: %lu)",
					col1, i - vres->row_pos
				);
			XSRETURN_YES;
		}
		else if( hv != NULL ) {
			if( col2 < 0 ) {
				col2 = vres->column_count + col2;
				if( col2 < 0 )
					col2 = 0;
			}
			else if( (u_long) col2 >= vres->column_count ) {
				col2 = vres->column_count - 1;
			}
			for( i = vres->row_pos; i < vres->row_count; i ++ ) {
				store = sv_2mortal( dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) );
				skey = SvPVx( store, lkey );
				(void) hv_store( hv, skey, (I32) lkey,
					dbe_vrt_res_fetch(
						type,
						vres->rows[i].data[col2],
						vres->rows[i].lengths[col2]
					), 0
				);
			}
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= col1: %d, col2: %d (rows: %lu)",
					col1, i - vres->row_pos
				);
			XSRETURN_YES;
		}
		else {
			for( i = vres->row_pos; i < vres->row_count; i ++ ) {
				XPUSHs( sv_2mortal( dbe_vrt_res_fetch(
					type,
					vres->rows[i].data[col1],
					vres->rows[i].lengths[col1]
				) ) );
			}
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetch_col= %lu (col: %d)", i - vres->row_pos, col1 );
		}
		break;
	}


#/*****************************************************************************
# * RES_fetchrow_arrayref( this )
# *****************************************************************************/

void
RES_fetchrow_arrayref( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow;
	u_long i;
	AV *av;
	SV *sv;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "res", res->con, "fetch_row" );
		if( dbe_res_fetch_row( res, FALSE ) != DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_arrayref= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchrow_arrayref= TRUE (columns: %u, row: %lu)",
				res->num_fields,
				dd->res_row_tell ? dd->res_row_tell( res->drv_data ) : 0
			);
		av = (AV *) sv_2mortal( (SV *) newAV() );
		for( i = 0; i < res->num_fields; i ++ ) {
			sv = res->sv_buffer[i] != NULL ? res->sv_buffer[i] : newSV(0);
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			av_push( av, sv );
		}
		ST(0) = sv_2mortal( newRV( (SV *) av ) );
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		if( vres->row_pos >= vres->row_count ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_arrayref= FALSE" );
			XSRETURN_EMPTY;
		}
		vrow = vres->rows + vres->row_pos ++;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchrow_arrayref= TRUE (columns: %u, row: %lu)",
				vres->column_count, vres->row_pos
			);
		av = (AV *) sv_2mortal( (SV *) newAV() );
		for( i = 0; i < (int) vres->column_count; i ++ ) {
			sv = dbe_vrt_res_fetch(
				vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
			if( res->sv_bind[i] != NULL )
				sv_setsv( res->sv_bind[i], sv );
			av_push( av, sv );
		}
		ST(0) = sv_2mortal( newRV( (SV *) av ) );
		break;
	}
	XSRETURN(1);


#/*****************************************************************************
# * RES_fetchrow_hashref( this [, name] )
# *****************************************************************************/

void
RES_fetchrow_hashref( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	u_long cn = 0;
	HV *hv = NULL;
	char *name;
	STRLEN name_len;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( items > 1 && SvPOK( ST(1) ) ) {
		name = SvPVx( ST(1), name_len );
		if( my_stristr( name, "LC" ) != NULL )
			cn = 1;
		else if( my_stristr( name, "UC" ) != NULL )
			cn = 2;
	}
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dbe_res_fetch_hash( this, res, &hv, cn ) == DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_hashref= TRUE (columns: %u, row: %lu)",
					res->num_fields,
					dd->res_row_tell ? dd->res_row_tell( res->drv_data ) : 0
				);
			XPUSHs( sv_2mortal( newRV( (SV *) hv ) ) );
		}
		else {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_hashref= FALSE" );
		}
		break;
	case RES_TYPE_VRT:
		if( dbe_vrt_res_fetch_hash( res, &hv, cn ) == DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_hashref= TRUE (columns: %u, row: %lu)",
					((dbe_vrt_res_t *) res->drv_data)->column_count,
					((dbe_vrt_res_t *) res->drv_data)->row_pos
				);
			XPUSHs( sv_2mortal( newRV( (SV *) hv ) ) );
		}
		else {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchrow_hashref= FALSE" );
		}
		break;
	}


#/*****************************************************************************
# * RES_fetchall_arrayref( this )
# *****************************************************************************/

void
RES_fetchall_arrayref( this, ... )
	SV *this;
PREINIT:
	dbe_res_t *res;
	int fn = 0;
	AV *av;
	SV *sv;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( items > 1 ) {
		if( SvROK( ST(1) ) ) {
			sv = SvRV( ST(1) );
			if( SvTYPE( sv ) == SVt_PVHV )
				fn = 1;
		}
	}
	switch( res->type ) {
	case RES_TYPE_DRV:
		if( dbe_res_fetchall_arrayref( this, res, &av, fn ) != DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchall_arrayref= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchall_arrayref= TRUE (rows: %d)", av_len( av ) + 1 );
		XPUSHs( sv_2mortal( newRV( (SV *) av ) ) );
		break;
	case RES_TYPE_VRT:
		if( dbe_vrt_res_fetchall_arrayref( res, &av, fn ) != DBE_OK ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- fetchall_arrayref= FALSE" );
			XSRETURN_EMPTY;
		}
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchall_arrayref= TRUE (rows: %d)", av_len( av ) + 1 );
		XPUSHs( sv_2mortal( newRV( (SV *) av ) ) );
		break;
	}


#/*****************************************************************************
# * RES_fetchall_hashref( this, key )
# *****************************************************************************/

void
RES_fetchall_hashref( this, key, ... )
	SV *this;
	SV *key;
PREINIT:
	dbe_res_t *res;
	dbe_str_t *fields = NULL;
	HV *hv;
	u_long ks, i;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( SvROK( key ) && (key = SvRV( key )) ) {
		AV *av;
		SV **psv;
		if( SvTYPE( key ) != SVt_PVAV ) {
			dbe_drv_set_error( res->con, -1, "Unknown type for key fields" );
			HANDLE_ERROR_XS( this, "res", res->con, "fetchall_hashref failed" );
		}
		av = (AV *) key;
		ks = av_len( av ) + 1;
		if( ! ks ) {
			dbe_drv_set_error( res->con, -1, "List of key fields is empty" );
			HANDLE_ERROR_XS( this, "res", res->con, "fetchall_hashref failed" );
		}
		Newx( fields, ks, dbe_str_t );
		for( i = 0; i < ks; i ++ ) {
			psv = av_fetch( av, i, 0 );
			if( psv != NULL )
				fields[i].value = SvPVx( (*psv), fields[i].length );
			else {
				fields[i].value = "";
				fields[i].length = 0;
			}
		}
	}
	else {
		ks = items - 1;
		Newx( fields, ks, dbe_str_t );
		for( i = 0; i < ks; i ++ )
			fields[i].value = SvPVx( ST(i + 1), fields[i].length );
	}
	switch( res->type ) {
	case RES_TYPE_DRV:
		if( dbe_res_fetchall_hashref( this, res, fields, ks, &hv ) != DBE_OK )
			goto error;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchall_hashref= TRUE" );
		ST(0) = sv_2mortal( newRV( (SV *) hv ) );
		break;
	case RES_TYPE_VRT:
		if( dbe_vrt_res_fetchall_hashref( res, fields, ks, &hv ) != DBE_OK )
			goto error;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- fetchall_hashref= TRUE" );
		ST(0) = sv_2mortal( newRV( (SV *) hv ) );
		break;
	}
	Safefree( fields );
	XSRETURN(1);
error:
	Safefree( fields );
	IF_TRACE_CON( res->con, DBE_TRACE_ALL )
		TRACE( res->con, DBE_TRACE_ALL, TRUE,
			"<- fetchall_hashref= FALSE" );
	HANDLE_ERROR_XS( this, "res", res->con, "fetchall_hashref failed" );


#/*****************************************************************************
# * RES_row_seek( this, offset )
# *****************************************************************************/

void
RES_row_seek( this, offset )
	SV *this;
	UV offset;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	xlong r;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_row_seek == NULL )
			NOFUNCTION_XS( this, "res", res->con, "row_seek" );
		r = dd->res_row_seek( res->drv_data, (u_xlong) offset );
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- row_seek= %lu (previous: %ld)", offset, r );
#if IVSIZE >= 8
		XPUSHs( sv_2mortal( newSViv( r ) ) );
#else
		if( r >= -2147483647 && r <= 2147483647 ) {
			XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
		}
		else {
			char tmp[21], *s1;
			s1 = my_ltoa( tmp, r, 10 );
			XPUSHs( sv_2mortal( newSVpvn( tmp, s1 - tmp ) ) );
		}
#endif
		break;
	case RES_TYPE_VRT:
		vres = (dbe_vrt_res_t *) res->drv_data;
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- row_seek= %lu (previous: %lu)", offset, vres->row_pos );
		XPUSHs( sv_2mortal( newSVuv( vres->row_pos ) ) );
		if( offset > vres->row_count )
			vres->row_pos = vres->row_count;
		else
			vres->row_pos = (u_long) offset;
		break;
	}


#/*****************************************************************************
# * RES_row_tell( this )
# *****************************************************************************/

void
RES_row_tell( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	xlong r;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_row_tell == NULL )
			NOFUNCTION_XS( this, "res", res->con, "row_tell" );
		r = dd->res_row_tell( res->drv_data );
#if IVSIZE >= 8
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE, "<- row_tell= %ld", r );
		XPUSHs( sv_2mortal( newSViv( r ) ) );
#else
		if( r >= -2147483647 && r <= 2147483647 ) {
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- row_tell= %ld", r );
			XPUSHs( sv_2mortal( newSViv( (IV) r ) ) );
		}
		else {
			char tmp[22], *s1;
			s1 = my_ltoa( tmp, r, 10 );
			IF_TRACE_CON( res->con, DBE_TRACE_ALL )
				TRACE( res->con, DBE_TRACE_ALL, TRUE,
					"<- row_tell= %s", tmp );
			XPUSHs( sv_2mortal( newSVpvn( tmp, s1 - tmp ) ) );
		}
#endif
		break;
	case RES_TYPE_VRT:
		IF_TRACE_CON( res->con, DBE_TRACE_ALL )
			TRACE( res->con, DBE_TRACE_ALL, TRUE,
				"<- row_tell= %lu",
				((dbe_vrt_res_t *) res->drv_data)->row_pos
			);
		XPUSHs( sv_2mortal( newSVuv(
			((dbe_vrt_res_t *) res->drv_data)->row_pos ) ) );
		break;
	}


#/*****************************************************************************
# * RES_dump( this )
# *****************************************************************************/

void
RES_dump( this, maxlen = 35, lsep = NULL, fsep = ", ", out = NULL, header = 1 )
	SV *this;
	unsigned int maxlen;
	char *lsep;
	char *fsep;
	SV *out;
	int header;
PREINIT:
	dbe_res_t *res;
	drv_def_t *dd;
	dbe_vrt_res_t *vres;
	dbe_vrt_row_t *vrow, *vrow_end;
	SV **row, *sv;
	u_long i, wc, is_utf8;
	u_long row_count = 0;
	size_t len;
	char *str, *s2, *s3;
	u_int pos;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	if( out == NULL )
		out = newRV_noinc( (SV *) PL_stderrgv );
	if( lsep == NULL )
		lsep = (char *) "\n";
	if( maxlen < 6 )
		maxlen = 6;
	maxlen -= 3;
	switch( res->type ) {
	case RES_TYPE_DRV:
		dd = res->con->drv->def;
		if( dd->res_fetch_row == NULL )
			NOFUNCTION_XS( this, "res", res->con, "res_fetch_row" );
		if( res->names == NULL )
			if( dbe_res_fetch_names( this, res ) != DBE_OK )
				XSRETURN_EMPTY;
		if( dd->res_row_tell != NULL &&
			dd->res_row_tell( res->drv_data ) > 0
		) {
			if( dd->res_row_seek != NULL )
				dd->res_row_seek( res->drv_data, 0 );
		}
		if( header ) {
			for( i = 0; i < res->num_fields; i ++ ) {
				if( i > 0 )
					dbe_perl_printf( out, "%s", fsep );
				dbe_perl_printf( out, "'%s'", res->names_conv[i].value );
			}
			dbe_perl_printf( out, "%s", lsep );
		}
		row = res->sv_buffer;
		while( dbe_res_fetch_row( res, FALSE ) == DBE_OK ) {
			ENTER;
			SAVETMPS;
			for( i = 0; i < res->num_fields; i ++ ) {
				if( i > 0 )
					dbe_perl_printf( out, "%s", fsep );
				if( (sv = row[i]) != NULL )
					sv_2mortal( sv );
				if( sv == NULL || ! SvOK( sv ) )
					dbe_perl_printf( out, "undef" );
				else {
					str = SvPV( sv, len );
					is_utf8 = 0;
					pos = 0;
					for( s2 = str, s3 = s2 + len; s2 < s3; s2 ++ ) {
						if( pos ++ == maxlen ) {
							my_strcpy( s2, "..." );
							break;
						}
						wc = *s2;
						if( wc & 0x80 ) {
							if( (s2[1] & 0xC0) != 0x80 )
								goto _no_utf8_1;
							wc &= 0x3F;
							if( wc & 0x20 ) {
								if( (s2[2] & 0xC0) != 0x80 )
									goto _no_utf8_1;
								s2 += 2;
							}
							is_utf8 = 1;
							s2 ++;
						}
						else if( wc > 127 ) {
_no_utf8_1:
							*s2 = '?';
						}
					}
					if( is_utf8 )
						dbe_perl_printf( out, "\"%s\"", str );
					else
						dbe_perl_printf( out, "'%s'", str );
				}
			}
			dbe_perl_printf( out, "%s", lsep );
			row_count ++;
			FREETMPS;
			LEAVE;
		}
		break;
	case RES_TYPE_VRT:
		vres = res->vrt_res;
		if( header ) {
			for( i = 0; i < vres->column_count; i ++ ) {
				if( i > 0 )
					dbe_perl_printf( out, "%s", fsep );
				dbe_perl_printf( out, "'%s'", vres->columns[i].name_conv );
			}
			dbe_perl_printf( out, "%s", lsep );
		}
		vrow = vres->rows;
		vrow_end = vres->rows + vres->row_count;
		for( ; vrow < vrow_end; vrow ++ ) {
			ENTER;
			SAVETMPS;
			for( i = 0; i < vres->column_count; i ++ ) {
				if( i > 0 )
					dbe_perl_printf( out, "%s", fsep );
				sv = dbe_vrt_res_fetch(
					vres->columns[i].type, vrow->data[i], vrow->lengths[i] );
				sv_2mortal( sv );
				if( sv == NULL || ! SvOK( sv ) )
					dbe_perl_printf( out, "undef" );
				else {
					str = SvPV( sv, len );
					is_utf8 = 0;
					pos = 0;
					for( s2 = str, s3 = s2 + len; s2 < s3; s2 ++ ) {
						if( pos ++ == maxlen ) {
							my_strcpy( s2, "..." );
							break;
						}
						wc = *s2;
						if( wc & 0x80 ) {
							if( (s2[1] & 0xC0) != 0x80 )
								goto _no_utf8_2;
							wc &= 0x3F;
							if( wc & 0x20 ) {
								if( (s2[2] & 0xC0) != 0x80 )
									goto _no_utf8_2;
								s2 += 2;
							}
							is_utf8 = 1;
							s2 ++;
						}
						else if( wc > 127 ) {
_no_utf8_2:
							*s2 = '?';
						}
					}
					if( is_utf8 )
						dbe_perl_printf( out, "\"%s\"", str );
					else
						dbe_perl_printf( out, "'%s'", str );
				}
			}
			dbe_perl_printf( out, "%s", lsep );
			row_count ++;
			FREETMPS;
			LEAVE;
		}
		break;
	}
	dbe_perl_printf( out, "%lu rows%s", row_count, lsep );


#/*****************************************************************************
# * RES_errno( this )
# *****************************************************************************/

void
RES_errno( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	XSRETURN_IV( (IV) res->con->last_errno );


#/*****************************************************************************
# * RES_error( this )
# *****************************************************************************/

void
RES_error( this )
	SV *this;
PREINIT:
	dbe_res_t *res;
PPCODE:
	if( (res = dbe_res_find( this )) == NULL )
		XSRETURN_EMPTY;
	ST(0) = sv_2mortal( newSVpvn(
		res->con->last_error, strlen( res->con->last_error ) ) );
	XSRETURN(1);


#/*########################### STATEMENT CLASS ###############################*/


MODULE = DBE		PACKAGE = DBE::STMT		PREFIX = STMT_


#/*****************************************************************************
# * STMT_DESTROY()
# *****************************************************************************/

void
STMT_DESTROY( ... )
PREINIT:
	dbe_stmt_t *stmt;
CODE:
	if( items < 1 || (stmt = dbe_stmt_find( ST(0) )) == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( "DESTROY called for stmt %lu, ref %d\n", stmt->id, stmt->refcnt ); 
#endif
	stmt->refcnt --;
	if( stmt->refcnt < 0 ) {
		IF_TRACE_CON( stmt->con, DBE_TRACE_ALL )
			TRACE( stmt->con, DBE_TRACE_ALL, TRUE,
				"<- DESTROY= %s=SCALAR(0x%x)",
				stmt->con->drv->class_stmt, SvRV( ST(0) )
			);
		dbe_stmt_rem( stmt );
	}


#/*****************************************************************************
# * STMT_close( this )
# *****************************************************************************/

void
STMT_close( this )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
CODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	IF_TRACE_CON( stmt->con, DBE_TRACE_ALL )
		TRACE( stmt->con, DBE_TRACE_ALL, TRUE,
			"<- DESTROY= %s=SCALAR(0x%x)",
			stmt->con->drv->class_stmt, SvRV( this )
		);
	dbe_stmt_rem( stmt );
	XSRETURN_YES;


#/*****************************************************************************
# * STMT_param_count( this )
# *****************************************************************************/

void
STMT_param_count( this )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
	drv_def_t *dd;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = stmt->con->drv->def;
	if( dd->stmt_param_count == NULL )
		NOFUNCTION_XS( this, "stmt", stmt->con, "param_count" );
	XSRETURN_IV( (IV) dd->stmt_param_count( stmt->drv_data ) );


#/*****************************************************************************
# * STMT_bind_param( this, p_num [, p_val [, p_type]] )
# *****************************************************************************/

void
STMT_bind_param( this, p_num, p_val = NULL, p_type = NULL )
	SV *this;
	SV *p_num;
	SV *p_val;
	SV *p_type;
PREINIT:
	dbe_stmt_t *stmt;
	drv_def_t *dd;
	int r = DBE_OK, i;
	char *name = NULL, type;
	dbe_param_t *param;
	STRLEN name_len;
	u_long pos;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	type = p_type != NULL && SvOK( p_type ) ? SvPV_nolen( p_type )[0] : 0;
	dd = stmt->con->drv->def;
	if( SvIOK( p_num ) ) {
		pos = (u_long) SvUV( p_num );
_bind_num:
		r = dbe_bind_param_num(
			this, "stmt", stmt->con, stmt->drv_data, dd,
			pos, p_val, type, name
		);
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			if( dbe_con_reconnect( this, "stmt", stmt->con ) == DBE_OK )
				dbe_drv_set_error( stmt->con, -1,
					"Connection restored, but statement is lost" );
		default:
			HANDLE_ERROR_XS( this, "stmt", stmt->con, "Bind param failed" );
		}
	}
	else if( SvPOK( p_num ) ) {
		name = SvPV( p_num, name_len );
		if( isDIGIT( *name ) ) {
			pos = (u_long) SvUV( p_num );
			goto _bind_num;
		}
		if( (param = stmt->params) == NULL ) {
			if( dd->stmt_param_position == NULL )
				NOFUNCTION_XS( this, "stmt", stmt->con, "param_position" );
			r = dd->stmt_param_position(
				stmt->drv_data, name, name_len, &pos );
			if( r != DBE_OK )
				HANDLE_ERROR_XS( this, "stmt", stmt->con, "Bind param failed" );
			goto _bind_num;
		}
		else {
			if( *name == ':' )
				name ++, name_len --;
			r = DBE_ERROR;
			if( (param = stmt->params) != NULL ) {
				for( i = 1; i <= stmt->num_params; i ++, param ++ ) {
					if( param->name != NULL &&
						my_stricmp( param->name, name ) == 0
					) {
						r = dbe_bind_param_num(
							this, "stmt", stmt->con, stmt->drv_data, dd,
							i, p_val, type, name
						);
						if( r != DBE_OK ) {
							HANDLE_ERROR_XS(
								this, "stmt", stmt->con, "Bind param failed" );
						}
						param->type = type;
					}
				}
			}
			if( r != DBE_OK ) {
				dbe_drv_set_error( stmt->con,
					-1, "Invalid parameter name \"%s\"", name );
				HANDLE_ERROR_XS( this, "stmt", stmt->con, "Bind param failed" );
			}
		}
	}
	else {
		dbe_drv_set_error( stmt->con, -1, "Invalid parameter identifier" );
		HANDLE_ERROR_XS( this, "stmt", stmt->con, "Bind param failed" );
	}
	XSRETURN_YES;


#/*****************************************************************************
# * STMT_bind( this )
# *****************************************************************************/

void
STMT_bind( this, ... )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
	drv_def_t *dd;
	u_long i;
	int r;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = stmt->con->drv->def;
	if( items <= 1 )
		XSRETURN_YES;
	for( i = 1; i < (u_long) items; i ++ ) {
		r = dbe_bind_param_num( this, "stmt",
			stmt->con, stmt->drv_data, dd, i, ST(i), '\0', NULL );
		switch( r ) {
		case DBE_OK:
			break;
		case DBE_CONN_LOST:
			/* statement is lost! */
			if( dbe_con_reconnect( this, "stmt", stmt->con ) == DBE_OK )
				dbe_drv_set_error( stmt->con, -1,
					"Connection restored, but statement is lost" );
		default:
			HANDLE_ERROR_XS( this, "stmt", stmt->con, "Bind param failed" );		
		}
	}


#/*****************************************************************************
# * STMT_execute( this )
# *****************************************************************************/

void
STMT_execute( this, ... )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
	dbe_res_t *res = NULL;
	drv_def_t *dd;
	SV *sv;
	HV *hv;
	char *key;
	int i, r, key_len;
	dbe_param_t *param;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	dd = stmt->con->drv->def;
	if( dd->stmt_execute == NULL )
		NOFUNCTION_XS( this, "stmt", stmt->con, "execute" );
	if( items > 1 ) {
		if( SvROK( ST(1) ) ) {
			sv = SvRV( ST(1) );
			if( SvTYPE( sv ) == SVt_PVHV ) {
				hv = (HV *) sv, hv_iterinit( hv );
				while( (sv = hv_iternextsv( hv, &key, &key_len )) != NULL ) {
					r = dbe_bind_param_str(
						this, "stmt", stmt->con, stmt->drv_data, dd,
						stmt->param_names, stmt->num_params, key, sv, 0
					);
					switch( r ) {
					case DBE_OK:
						break;
					case DBE_CONN_LOST:
						goto _reconnect;
					default:
						goto _error;
					}
				}
				goto _bind_inout;
			}
		}
		for( i = 1; i < items; i ++ ) {
			r = dbe_bind_param_num(
				this, "stmt", stmt->con, stmt->drv_data, dd,
				i, ST(i), 0, NULL
			);
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _error;
			}
		}
	}
_bind_inout:
	if( stmt->params != NULL ) {
		param = stmt->params;
		for( i = 1; i <= stmt->num_params; i ++, param ++ ) {
			if( param->sv == NULL )
				continue;
			r = dbe_bind_param_num(
				this, "stmt", stmt->con, stmt->drv_data, dd,
				i, param->sv, param->type, param->name
			);
			switch( r ) {
			case DBE_OK:
				break;
			case DBE_CONN_LOST:
				goto _reconnect;
			default:
				goto _error;
			}
		}
	}
	switch( dd->stmt_execute( stmt->drv_data, &res ) ) {
	case DBE_OK:
		if( res == NULL ) {
			IF_TRACE_CON( stmt->con, DBE_TRACE_SQLFULL )
				TRACE( stmt->con, DBE_TRACE_SQLFULL, TRUE, "<- execute= TRUE" );
			XSRETURN_YES;
		}
		if( stmt->params != NULL ) {
			dbe_param_t *param = stmt->params;
			for( i = 0; i < stmt->num_params; i ++, param ++ ) {
				if( i >= (int) res->num_fields )
					break;
				if( param->sv != NULL )
					res->sv_bind[i] = SvREFCNT_inc( param->sv );
			}
		}
		dbe_res_add( stmt->con, res );
		hv = gv_stashpv( stmt->con->drv->class_res, TRUE );
		sv = sv_2mortal( newSViv( (IV) res->id ) );
		IF_TRACE_CON( stmt->con, DBE_TRACE_SQLFULL )
			TRACE( stmt->con, DBE_TRACE_SQLFULL, TRUE,
				"<- execute= %s=SCALAR(0x%lx)",
				stmt->con->drv->class_res, (size_t) sv
			);
		XPUSHs( sv_2mortal( sv_bless( newRV( sv ), hv ) ) );
		break;
	case DBE_CONN_LOST:
		/* statement is lost! */
_reconnect:
		if( dbe_con_reconnect( this, "stmt", stmt->con ) == DBE_OK )
			dbe_drv_set_error( stmt->con, -1,
				"Connection restored, but statement is lost" );
	default:
_error:
		IF_TRACE_CON( stmt->con, DBE_TRACE_SQLFULL )
			TRACE( stmt->con, DBE_TRACE_SQLFULL, TRUE, "<- execute= FALSE" );
		HANDLE_ERROR_XS( this, "stmt", stmt->con, "Execute failed" );
	}


#/*****************************************************************************
# * STMT_errno( this )
# *****************************************************************************/

void
STMT_errno( this )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	XSRETURN_IV( (IV) stmt->con->last_errno );


#/*****************************************************************************
# * STMT_error( this )
# *****************************************************************************/

void
STMT_error( this )
	SV *this;
PREINIT:
	dbe_stmt_t *stmt;
PPCODE:
	if( (stmt = dbe_stmt_find( this )) == NULL )
		XSRETURN_EMPTY;
	ST(0) = sv_2mortal( newSVpvn(
		stmt->con->last_error, strlen( stmt->con->last_error ) ) );
	XSRETURN(1);


#/*########################### LOB STREAM CLASS ##############################*/


MODULE = DBE		PACKAGE = DBE::LOB		PREFIX = LOB_

void
LOB_DESTROY( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	dbe_con_t *con;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu destroy, refcnt %d\n", ls->id, ls->refcnt );
#endif
	ls->refcnt --;
	if( ls->refcnt < 0 ) {
		IF_TRACE_CON( con, DBE_TRACE_ALL )
			TRACE( con, DBE_TRACE_ALL, TRUE,
				"<- DESTROY= DBE::LOB=SCALAR(0x%x)", SvRV( this ) );
		dbe_lob_stream_rem( con, ls );
	}
	XSRETURN_EMPTY;


void
LOB_close( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	dbe_con_t *con;
	drv_def_t *dd;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu close\n", ls->id );
#endif
	if( ! ls->closed ) {
		dd = con->drv->def;
		if( dd->con_lob_close != NULL )
			dd->con_lob_close( con->drv_data, ls->context );
		ls->closed = TRUE;
	}
	XSRETURN_YES;


void
LOB_tell( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
PPCODE:
	ls = dbe_lob_stream_find( this, NULL );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu tell\n", ls->id );
#endif
	XSRETURN_IV( ls->offset );


void
LOB_seek( this, position, whence, ... )
	SV *this;
	int position;
	int whence;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu seek position %d, whence %d\n",
		ls->id, position, whence );
#endif
	dd = con->drv->def;
	if( ls->size < 0 ) {
		if( dd->con_lob_size == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_size" );
			goto _error;
		}
		ls->size = dd->con_lob_size( con->drv_data, ls->context );
		if( ls->size == DBE_NO_DATA )
			dbe_drv_set_error( con, -1, "Unknown size of LOB" );
		if( ls->size < 0 )
			goto _error;
	}
	switch( whence ) {
	case SEEK_SET:
		ls->offset = position;
		break;
	case SEEK_CUR:
		ls->offset += position;
		break;
	case SEEK_END:
		ls->offset = ls->size + position;
		break;
	default:
		dbe_drv_set_error( con,
			-1, "Invalid value for parameter whence: %d", whence );
		goto _error;
	}
	if( ls->offset > ls->size )
		ls->offset = ls->size;
	else if( ls->offset < 0 )
		ls->offset = 0;
	XSRETURN_YES;
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Seek LOB failed" );


void
LOB_size( this )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu size\n", ls->id );
#endif
	dd = con->drv->def;
	if( ls->size < 0 ) {
		if( dd->con_lob_size == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_size" );
			goto _error;
		}
		ls->size = dd->con_lob_size( con->drv_data, ls->context );
		if( ls->size == DBE_NO_DATA )
			dbe_drv_set_error( con, -1, "Unknown size of LOB" );
		if( ls->size < 0 )
			goto _error;
	}
	XSRETURN_IV( ls->size );
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Get size of LOB failed" );


void
LOB_eof( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu eof\n", ls->id );
#endif
	dd = con->drv->def;
	if( ls->size < 0 ) {
		if( dd->con_lob_size == NULL ) {
			dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_size" );
			goto _error;
		}
		ls->size = dd->con_lob_size( con->drv_data, ls->context );
		if( ls->size == DBE_NO_DATA )
			dbe_drv_set_error( con, -1, "Unknown size of LOB" );
		if( ls->size < 0 )
			goto _error;
	}
	XSRETURN_IV( ls->offset >= ls->size );
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Get end of LOB failed" );


void
LOB_getc( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	char tmp[8], *s1;
	int length, ch;
	SV *err;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu getc\n", ls->id );
#endif
	dd = con->drv->def;
	if( dd->con_lob_read == NULL ) {
		dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_read" );
		goto _error;
	}
	length = dd->con_lob_read(
		con->drv_data, ls->context, &ls->offset, tmp, 1 );
	if( length == DBE_ERROR )
		goto _error;
	if( length == DBE_NO_DATA )
		XSRETURN_IV( -1 );
	ch = 0;
	for( s1 = tmp; length; length --, s1 ++ ) {
		ch <<= 8;
		ch |= (unsigned char) *s1;
	}
	XSRETURN_IV( ch );
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Read LOB failed" );


void
LOB_read( this, scalar, length, ... )
	SV *this;
	SV *scalar;
	int length;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
	char *buf;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu read %ld\n", ls->id, length );
#endif
	if( ! SvPOK( scalar ) )
		sv_setpvn( scalar, "", 0 );
	SvGROW( scalar, (STRLEN) length + 1 );
	buf = SvPV_nolen( scalar );
	dd = con->drv->def;
	if( dd->con_lob_read == NULL ) {
		dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_read" );
		goto _error;
	}
	length = dd->con_lob_read(
		con->drv_data, ls->context, &ls->offset, buf, length
	);
	if( length == DBE_ERROR )
		goto _error;
	if( length == DBE_NO_DATA )
		length = 0;
	buf[length] = '\0';
	SvCUR_set( scalar, length );
	XSRETURN_IV( length );
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	*buf = '\0';
	SvCUR_set( scalar, 0 );
	HANDLE_ERROR_XS( this, "lob", con, "Read LOB failed" );


void
LOB_print( this, ... )
	SV *this;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
	int i, r;
	char *str;
	STRLEN str_len;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu print %d items\n", ls->id, items - 1 );
#endif
	dd = con->drv->def;
	if( dd->con_lob_write == NULL ) {
		dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_write" );
		goto _error;
	}
	for( i = 1; i < items; i ++ ) {
		str = SvPV( ST(i), str_len );
		r = dd->con_lob_write(
			con->drv_data, ls->context, &ls->offset, str, str_len );
		if( r < 0 )
			goto _error;
	}
	XSRETURN_YES;
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Write LOB failed" );


void
LOB_write( this, buffer, ... )
	SV *this;
	SV *buffer;
PREINIT:
	dbe_lob_stream_t *ls;
	drv_def_t *dd;
	dbe_con_t *con;
	SV *err;
	int r;
	char *str;
	STRLEN str_len, x;
PPCODE:
	ls = dbe_lob_stream_find( this, &con );
	if( ls == NULL )
		XSRETURN_EMPTY;
	str = SvPVbyte( buffer, str_len );
#ifdef DBE_DEBUG
	_debug( ">>>>>> lob stream %lu write %u bytes\n", ls->id, str_len );
#endif
	if( items > 2 ) {
		x = SvUV( ST(2) );
		if( x <= str_len )
			str += x, str_len -= x;
		else
			str += str_len, str_len = 0;
	}
	if( items > 3 ) {
		x = SvUV( ST(3) );
		if( x <= str_len )
			str_len -= x;
		else
			str_len = 0;
	}
	dd = con->drv->def;
	if( dd->con_lob_write == NULL ) {
		dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, "lob_write" );
		goto _error;
	}
	r = dd->con_lob_write(
		con->drv_data, ls->context, &ls->offset, str, str_len );
	if( r < 0 )
		goto _error;
	XSRETURN_YES;
_error:
	err = get_sv( "!", TRUE );
	sv_setiv( err, (IV) con->last_errno );
	sv_setpv( err, con->last_error );
	SvIOK_on( err );
	HANDLE_ERROR_XS( this, "lob", con, "Write LOB failed" );

