#ifndef __DBE_DEF_H__
#define __DBE_DEF_H__ 1

#include "dbe.h"
#include "dbe_config.h"
#include "dbe_build.h"

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

#if (PERL_VERSION <= 8)
#define SvREFCNT_inc_NN		SvREFCNT_inc
#endif

#ifdef DBE_DEBUG
EXTERN int my_debug( const char *fmt, ... );
#define _debug my_debug
#endif

#if DBE_DEBUG > 1

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef memcpy
#undef memset

#undef Newx
#undef Newxz
#undef Safefree
#undef Renew

extern HV				*hv_dbg_mem;
extern perl_mutex		dbg_mem_lock;

void debug_init();
void debug_free();

#define Newx(v,n,t) { \
	char __v[41], __msg[128]; \
	MUTEX_LOCK( &dbg_mem_lock ); \
	(v) = ((t*) safemalloc( (size_t) (n) * sizeof(t) )); \
	sprintf( __v, "0x%lx", (size_t) (v) ); \
	sprintf( __msg, "0x%lx malloc(%lu * %lu) called at %s:%d", \
		(size_t) (v), (size_t) (n), sizeof(t), __FILE__, __LINE__ ); \
	_debug( "%s\n", __msg ); \
	(void) hv_store( hv_dbg_mem, \
		__v, (I32) strlen( __v ), newSVpvn( __msg, strlen( __msg ) ), 0 ); \
	MUTEX_UNLOCK( &dbg_mem_lock ); \
}

#define Newxz(v,n,t) { \
	char __v[41], __msg[128]; \
	MUTEX_LOCK( &dbg_mem_lock ); \
	(v) = ((t*) safecalloc( (size_t) (n), sizeof(t) )); \
	sprintf( __v, "0x%lx", (size_t) (v) ); \
	sprintf( __msg, "0x%lx calloc(%lu * %lu) called at %s:%d", \
		(size_t) (v), (size_t) (n), sizeof(t), __FILE__, __LINE__ ); \
	_debug( "%s\n", __msg ); \
	(void) hv_store( hv_dbg_mem, \
		__v, (I32) strlen( __v ), newSVpvn( __msg, strlen( __msg ) ), 0 ); \
	MUTEX_UNLOCK( &dbg_mem_lock ); \
}

#define Safefree(x) { \
	char __v[41]; \
	MUTEX_LOCK( &dbg_mem_lock ); \
	if( (x) != NULL ) { \
		sprintf( __v, "0x%lx", (size_t) (x) ); \
		_debug( "0x%lx free() called at %s:%d\n", \
			(size_t) (x), __FILE__, __LINE__ ); \
		(void) hv_delete( hv_dbg_mem, __v, (I32) strlen( __v ), G_DISCARD ); \
		safefree( (x) ); (x) = NULL; \
	} \
	MUTEX_UNLOCK( &dbg_mem_lock ); \
}

#define Renew(v,n,t) { \
	register void *__p = (v); \
	char __v[41], __msg[128]; \
	MUTEX_LOCK( &dbg_mem_lock ); \
	sprintf( __v, "0x%lx", (size_t) (v) ); \
	(void) hv_delete( hv_dbg_mem, __v, (I32) strlen( __v ), G_DISCARD ); \
	(v) = ((t*) saferealloc( __p, (size_t) (n) * sizeof(t) )); \
	sprintf( __v, "0x%lx", (size_t) (v) ); \
	sprintf( __msg, "0x%lx realloc(0x%lx, %lu * %lu) called at %s:%d", \
		(size_t) (v), (size_t) __p, (size_t) (n), sizeof(t), \
		__FILE__, __LINE__ ); \
	_debug( "%s\n", __msg ); \
	(void) hv_store( hv_dbg_mem, \
		__v, (I32) strlen( __v ), newSVpvn( __msg, strlen( __msg ) ), 0 ); \
	MUTEX_UNLOCK( &dbg_mem_lock ); \
}

#endif /* DBE_DEBUG > 1 */

#ifdef _WIN32
#undef vsnprintf
#define vsnprintf _vsnprintf
#endif

#define DBE_TYPE_NAME(t) ( \
	((t) == DBE_TYPE_VARCHAR)			? "varchar" : \
	((t) == DBE_TYPE_INTEGER)			? "integer" : \
	((t) == DBE_TYPE_DOUBLE)			? "double" : \
	((t) == DBE_TYPE_BINARY)			? "binary" : \
	((t) == DBE_TYPE_BIGINT)			? "bigint" : \
										"unknown" )

typedef struct st_dbe_vrt_row		dbe_vrt_row_t;
typedef struct st_dbe_vrt_column	dbe_vrt_column_t;
typedef struct st_dbe_vrt_res		dbe_vrt_res_t;
typedef struct st_dbe_lob_stream	dbe_lob_stream_t;
typedef struct st_dbe_param			dbe_param_t;
typedef struct st_dbe_stmt			dbe_stmt_t;
typedef struct st_dbe_global		dbe_global_t;

struct st_dbe_drv {
	struct st_dbe_drv				*prev;
	struct st_dbe_drv				*next;
	char							*name;
	drv_def_t						*def;
	char							*class_con;
	char							*class_res;
	char							*class_stmt;
};

struct st_dbe_vrt_row {
	char							**data;
	size_t							*lengths;
};

struct st_dbe_vrt_column {
	enum dbe_type					type;
	char							*name;
	size_t							name_length;
	char							*name_conv;
	size_t							name_conv_length;
};

struct st_dbe_vrt_res {
	dbe_vrt_column_t				*columns;
	u_long							column_count;
	u_long							column_pos;
	dbe_vrt_row_t					*rows;
	u_long							row_pos;
	u_long							row_count;
};

#define RES_TYPE_DRV				1
#define RES_TYPE_VRT				2

#define RES_FREE_NAMES				1

struct st_dbe_res {
	char							type;
	struct st_dbe_res				*prev;
	struct st_dbe_res				*next;
	u_long							id;
	u_long							num_fields;
	SV								**sv_buffer;
	u_long							bind_count;
	SV								**sv_bind;
	dbe_str_t						*names;
	dbe_str_t						*names_conv;
	u_long							flags;								
	struct st_dbe_con				*con;
	union {
		drv_res_t					*drv_data;
		dbe_vrt_res_t				*vrt_res;
	};
	int								refcnt;
};

struct st_dbe_lob_stream {
	dbe_lob_stream_t				*prev;
	dbe_lob_stream_t				*next;
	u_long							id;
	void							*context;
	int								offset;
	int								size;
	int								refcnt;
	char							closed;
};

struct st_dbe_param {
	SV								*sv;
	char							type;
	char							*name;
};

struct st_dbe_stmt {
	dbe_stmt_t						*prev;
	dbe_stmt_t						*next;
	dbe_con_t						*con;
	u_long							id;
	drv_stmt_t						*drv_data;
	int								refcnt;
	dbe_param_t						*params;
	char							**param_names;
	int								num_params;
};

#define CON_ONERRORDIE				1
#define CON_ONERRORWARN				2
#define CON_NAME_LC					4
#define CON_NAME_UC					8

struct st_dbe_con {
	struct st_dbe_con				*prev;
	struct st_dbe_con				*next;
	u_long							id;
	dbe_res_t						*first_res;
	dbe_res_t						*last_res;
	dbe_stmt_t						*first_stmt;
	dbe_stmt_t						*last_stmt;
	drv_con_t						*drv_data;
	dbe_drv_t						*drv;
	char							last_error[1024];
	int								last_errno;
	unsigned int					flags;
	unsigned int					reconnect_count;
	char							catalog_separator;
	char							quote_id_prefix;
	char							quote_id_suffix;
	char							escape_pattern;
	char							catalog_location;
	SV								*error_handler;
	SV								*error_handler_arg;
	int								refcnt;
	SV								*sv_trace_out;
	int								trace_level;
	int								trace_local;
	SV								*this;
#ifdef USE_ITHREADS
	perl_mutex						thread_lock;
#endif
	dbe_lob_stream_t				*first_lob;
	dbe_lob_stream_t				*last_lob;
};

struct st_dbe_global {
	dbe_drv_t						*first_drv;
	dbe_drv_t						*last_drv;
	dbe_con_t						*first_con;
	dbe_con_t						*last_con;
	char							last_error[1024];
	int								last_errno;
	int								cleanup;
	int								destroyed;
	u_long							counter;
	SV								*sv_trace_out;
	int								trace_level;
#ifdef USE_ITHREADS
	perl_mutex						thread_lock;
#if DBE_DEBUG > 1
	int								lock_counter;
#endif
#endif
};

#ifdef USE_ITHREADS

#if DBE_DEBUG > 1

#define GLOBAL_LOCK() { \
	_debug( "enter global lock (%d) at %s:%d\n", \
		global.lock_counter + 1, __FILE__, __LINE__ ); \
	MUTEX_LOCK( &global.thread_lock ); \
	global.lock_counter ++; \
}

#define GLOBAL_UNLOCK() { \
	_debug( "leave global lock (%d) at %s:%d\n", \
		global.lock_counter, __FILE__, __LINE__ ); \
	MUTEX_UNLOCK( &global.thread_lock ); \
	global.lock_counter --; \
}

#define CON_LOCK(con) { \
	_debug( "enter con (%u) lock at %s:%d\n", con->id, __FILE__, __LINE__ ); \
	MUTEX_LOCK( &con->thread_lock ); \
}

#define CON_UNLOCK(con) { \
	_debug( "leave con (%u) lock at %s:%d\n", con->id, __FILE__, __LINE__ ); \
	MUTEX_UNLOCK( &con->thread_lock ); \
}

#else /*DBE_DEBUG > 1*/

#define GLOBAL_LOCK()			MUTEX_LOCK( &global.thread_lock )
#define GLOBAL_UNLOCK()			MUTEX_UNLOCK( &global.thread_lock )

#define CON_LOCK(con)			MUTEX_LOCK( &con->thread_lock )
#define CON_UNLOCK(con)			MUTEX_UNLOCK( &con->thread_lock )

#endif /*DBE_DEBUG > 1*/

#else /*USE_ITHREADS*/

#define GLOBAL_LOCK()			{}
#define GLOBAL_UNLOCK()			{}
#define CON_LOCK(con)			{}
#define CON_UNLOCK(con)			{}

#endif /*USE_ITHREADS*/

#define NOFIELDS_ERROR		"Driver did not return a field count for the" \
							" result set"

#define NOFIELDS_XS(this, obj_id, con, action) { \
	dbe_drv_set_error( con, -1, NOFIELDS_ERROR ); \
	HANDLE_ERROR( this, obj_id, con, action ); \
	XSRETURN_EMPTY; \
}

#define NOFUNCTION_ERROR	"Driver does not implement \"%s()\""
#define NOFUNCTION_ACTION	"Undefined function"

#define NOFUNCTION(this, obj_id, con, fnc) { \
	SV *__this = this; \
	dbe_drv_set_error( con, -1, NOFUNCTION_ERROR, fnc ); \
	HANDLE_ERROR( __this, obj_id, con, NOFUNCTION_ACTION ); \
}

#define NOFUNCTION_XS(this, obj_id, con, fnc) { \
	NOFUNCTION( this, obj_id, con, fnc ); \
	XSRETURN_EMPTY; \
}

#define NOFUNCTION_C(this, obj_id, con, fnc) { \
	NOFUNCTION( this, obj_id, con, fnc ); \
	return DBE_ERROR; \
}

#define HANDLE_ERROR(this, obj_id, con, action) \
	dbe_handle_error( this, obj_id, con, action )

#define HANDLE_ERROR_XS(this, obj_id, con, action) { \
	HANDLE_ERROR( this, obj_id, con, action ); \
	XSRETURN_EMPTY; \
}

#define STR_TRUE(str) \
	( (str) != NULL && (str)[0] != '\0' && (str)[0] != '0' \
		&& (str)[0] != 'f' && (str)[0] != 'F' \
		&& (str)[0] != 'n' && (str)[0] != 'N' )

#define BOOL_STR(x)		(x) ? "True" : "False"
#define ONOFF_STR(x)	(x) ? "On" : "Off"

extern dbe_global_t global;
extern dbe_t g_dbe;

/* global */
EXTERN dbe_drv_t *dbe_init_driver( const char *name, size_t name_len );
EXTERN void dbe_free();
EXTERN void dbe_cleanup();

/* error */
EXTERN void dbe_drv_set_error(
	dbe_con_t *dbe_con, int code, const char *msg, ... );
void dbe_handle_error(
	SV *obj, const char *obj_id, dbe_con_t *con, const char *action
);

/* trace */
EXTERN void dbe_drv_trace(
	dbe_con_t *dbe_con, int level, int show_caller, const char *msg, ...
);
const char *dbe_trace_getinfo_name( int code );
EXTERN int dbe_trace_level_by_name( const char *name );
EXTERN const char *dbe_trace_name_by_level( int level );
#define TRACE dbe_drv_trace
#define IF_TRACE_CON(con,level) \
	if( (con->trace_local && con->trace_level >= level) \
		|| (! con->trace_local && global.trace_level >= level) )
#define IF_TRACE_GLOBAL(level) \
	if( global.trace_level >= level )

/* misc */
EXTERN int dbe_dsn_parse( char *dsn, char ***pargs );
EXTERN SV *dbe_quote( const char *str, size_t str_len );
EXTERN SV *dbe_quote_id(
	const char **args, int argc,
	const char quote_id_prefix, const char quote_id_suffix,
	const char catalog_sep, const short catalog_location
);
EXTERN SV *dbe_escape_pattern(
	const char *str, size_t str_len, const char esc );
EXTERN char *dbe_convert_query(
	dbe_con_t *con, const char *query, size_t query_len, size_t *res_len,
	char ***p_params, int *p_param_count
);
EXTERN enum sql_type dbe_sql_type_by_name( const char *name );
EXTERN void dbe_perl_print( SV *sv, const char *msg, size_t msg_len );
EXTERN void dbe_perl_printf( SV *sv, const char *msg, ... );
char *my_dump_var( SV *sv, const char *name, size_t *rlen, int print );

/* lob stream */
EXTERN void *dbe_lob_stream_find( SV *sv, dbe_con_t **pcon );
EXTERN void dbe_lob_stream_rem( dbe_con_t *con, dbe_lob_stream_t *ls );
EXTERN void dbe_lob_stream_free( dbe_con_t *con, dbe_lob_stream_t *ls );

/* connection */
EXTERN void dbe_con_add( dbe_con_t *con );
EXTERN void dbe_con_rem( dbe_con_t *con );
EXTERN void dbe_con_free( dbe_con_t *con );
EXTERN void dbe_con_cleanup( dbe_con_t *con );
EXTERN dbe_con_t *dbe_con_find( SV *sv );
EXTERN dbe_con_t *dbe_con_find_by_id( SV *sv, u_long id );
EXTERN int dbe_con_reconnect( SV *obj, const char *obj_id, dbe_con_t *con );

/* result set */
EXTERN void dbe_res_add( dbe_con_t *con, dbe_res_t *res );
EXTERN void dbe_res_rem( dbe_res_t *res );
EXTERN void dbe_res_free_vrt( dbe_res_t *res );
EXTERN void dbe_res_free_drv( dbe_res_t *res );
EXTERN void dbe_res_free( dbe_res_t *res );
EXTERN dbe_res_t *dbe_res_find( SV *sv );
EXTERN int dbe_res_fetch_names( SV *this, dbe_res_t *res );
EXTERN int dbe_res_fetch_row( dbe_res_t *res, int names );
EXTERN int dbe_res_fetch_hash( SV *this, dbe_res_t *res, HV **phv, int nc );
EXTERN int dbe_res_fetchall_arrayref(
	SV *this, dbe_res_t *res, AV **pav, int fn );
EXTERN int dbe_res_fetchall_hashref(
	SV *this, dbe_res_t *res, dbe_str_t *fields, u_long ks, HV **phv );

/* statement */
EXTERN void dbe_stmt_add( dbe_con_t *con, dbe_stmt_t *stmt );
EXTERN void dbe_stmt_rem( dbe_stmt_t *stmt );
EXTERN void dbe_stmt_free( dbe_stmt_t *stmt );
EXTERN dbe_stmt_t *dbe_stmt_find( SV *sv );
EXTERN int dbe_bind_param_num(
	SV *obj, const char *obj_id, dbe_con_t *con, drv_stmt_t *stmt,
	drv_def_t *dd, int p_num, SV *p_val, char p_type, const char *p_name
);
EXTERN int dbe_bind_param_inout(
	SV *obj, const char *obj_id, dbe_con_t *con, drv_stmt_t *stmt,
	drv_def_t *dd, int p_num, SV *p_var, char p_type, const char *p_name
);
EXTERN int dbe_bind_param_str(
	SV *obj, const char *obj_id, dbe_con_t *con, drv_stmt_t *stmt,
	drv_def_t *dd, char **params, int param_count,
	char *p_num, SV *p_val, char p_type
);

/* virtual result set */
EXTERN SV *dbe_vrt_res_fetch( int type, const char *data, size_t data_len );
EXTERN int dbe_vrt_res_fetch_hash( dbe_res_t *res, HV **phv, int nc );
EXTERN int dbe_vrt_res_fetchall_arrayref( dbe_res_t *res, AV **pav, int fn );
EXTERN int dbe_vrt_res_fetchall_hashref(
	dbe_res_t *res, dbe_str_t *fields, u_long ks, HV **phv );

/* common */
EXTERN char *my_strncpy( char *dst, const char *src, size_t len );
EXTERN char *my_strcpy( char *dst, const char *src );
EXTERN char *my_strcpyl( char *dst, const char *src );
EXTERN char *my_strcpyu( char *dst, const char *src );
EXTERN char *my_itoa( char *str, long value, int radix );
EXTERN char *my_ltoa( char *str, xlong value, int radix );
EXTERN char *my_ultoa( char *str, u_xlong value, int radix );
EXTERN char *my_ftoa( register char *buf, double f );
EXTERN int my_stricmp( const char *cs, const char *ct );
EXTERN int my_strnicmp( const char *cs, const char *ct, size_t len );
EXTERN char *my_stristr( const char *str1, const char *str2 );

/* unicode */
EXTERN int my_utf8_to_unicode(
	const char *src, size_t src_len, char *dst, size_t dst_max );
EXTERN int my_unicode_to_utf8(
	const char *src, size_t src_len, char *dst, size_t dst_max );
EXTERN int my_utf8_toupper(
	const char *src, size_t src_len, char *dst, size_t dst_len );
EXTERN int my_utf8_tolower(
	const char *src, size_t src_len, char *dst, size_t dst_len );
EXTERN unsigned int my_towupper( unsigned int c );
EXTERN unsigned int my_towlower( unsigned int c );

#endif /* __DBE_DEF_H__ */
