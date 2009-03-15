#ifndef __COMMON_H__
#define __COMMON_H__ 1

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

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

/* setup debugging */
#ifdef CSV_DEBUG
int my_debug( const char *fmt, ... );
#define _debug	my_debug
#endif

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

#if CSV_DEBUG > 1

extern HV					*hv_dbg_mem;
extern perl_mutex			dbg_mem_lock;

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
		(size_t) (v), (size_t) __p, (size_t) (n), sizeof(t), __FILE__, __LINE__ ); \
	_debug( "%s\n", __msg ); \
	(void) hv_store( hv_dbg_mem, \
		__v, (I32) strlen( __v ), newSVpvn( __msg, strlen( __msg ) ), 0 ); \
	MUTEX_UNLOCK( &dbg_mem_lock ); \
}

#else /* CSV_DEBUG > 1 */

#define Newx(v,c,t) \
	( (v) = ( (t*) safemalloc( (c) * sizeof(t) ) ) )

#define Newxz(v,c,t) \
	( (v) = ( (t*) safecalloc( (c), sizeof(t) ) ) )

#define Safefree(x) \
	if( (x) != NULL ) { safefree( (x) ); (x) = NULL; }

#define Renew(v,n,t) \
	( (v) = ( (t*) saferealloc( (void *) (v), (n) * sizeof(t) ) ) )

#endif /* CSV_DEBUG > 1 */

#ifdef _WIN32
#undef vsnprintf
#define vsnprintf _vsnprintf
#endif

#ifdef _WIN32
#define EOL						"\r\n"
#else
#define EOL						"\n"
#endif

#ifdef USE_PERLIO

#undef FILE
#undef stdin
#undef stdout
#undef stderr
#undef fopen
#undef fclose
#undef fflush
#undef fgetc
#undef fgets
#undef fputc
#undef fputs
#undef fwrite
#undef fread
#undef fprintf
#undef vfprintf
#undef feof
#undef fseek
#undef rewind
#undef ferror
#undef clearerr

#define FILE				PerlIO
#define stdin				PerlIO_stdin()
#define stdout				PerlIO_stdout()
#define stderr				PerlIO_stderr()
#define fopen(f,m)			PerlIO_open( (f), (m) )
#define fclose(s)			PerlIO_close( (s) )
#define fflush(s)			PerlIO_flush( (s) )
#define fgetc(s)			PerlIO_getc( (s) )
#define fgets(p,n,s)		PerlIO_getc( (s), (p), (n) )
#define fputc(c,s)			PerlIO_putc( (s), (c) )
#define fputs(p,s)			PerlIO_puts( (s), (p) )
#define fwrite(p,l,n,s)		PerlIO_write( (s), (p), (l) * (n) )
#define fread(p,l,n,s)		PerlIO_read( (s), (p), (l) * (n) )
#define fprintf				PerlIO_printf
#define vfprintf			PerlIO_vprintf
#define feof(s)				PerlIO_eof( (s) )
#define fseek(s,n,w)		PerlIO_seek( (s), (n), (w) )
#define rewind(s)			PerlIO_rewind( (s) )
#define ferror(s)			PerlIO_error( (s) )
#define clearerr(s)			PerlIO_clearerr( (s) )

char *PerlIO_gets( PerlIO *stream, char *buf, int max );

#endif

typedef unsigned char byte;
#ifndef __unix__
typedef unsigned int uint;
#endif

#undef BYTE
#define BYTE unsigned char
#undef WORD
#define WORD unsigned short
#undef DWORD
#define DWORD unsigned long

#undef MAX
#define MAX(x,y) ( (x) < (y) ? (y) : (x) )
#undef MIN
#define MIN(x,y) ( (x) < (y) ? (x) : (y) )

#define ARRAY_LEN(x) (sizeof( (x) ) / sizeof( (x)[0] ))

extern const char CHAR_FROM_HEX[];
extern const char *HEX_FROM_CHAR;
extern const char ANSI_UCASE[];
extern const char ANSI_LCASE[];

#define TOUPPER(c)	ANSI_UCASE[(unsigned char)(c)]
#define TOLOWER(c)	ANSI_LCASE[(unsigned char)(c)]

#define STRICMP(a,b,r) \
	do { \
		register char *__sc = (a), *__st = (b), __r; \
		do { \
			if( (__r = TOUPPER( *__sc ) - TOUPPER( *__st ++ )) != 0 || ! *__sc ++ ) \
				break; \
		} while( 1 ); \
		(r) = __r; \
	} while( 0 )

EXTERN char *my_strtolower( register char *a );
EXTERN char *my_strtoupper( register char *a );
EXTERN char *my_stristr( register const char *str, const char *search );
EXTERN int my_stricmp( register const char *s, register const char *t );
EXTERN int my_strnicmp( const char *cs, const char *ct, size_t len );
EXTERN int my_strncmp( const char *cs, const char *ct, size_t len );
EXTERN char *my_strncpyu( register char *dst, register const char *src, size_t len );
EXTERN char *my_strcpy( register char *dst, register const char *src );
EXTERN char *my_strncpy( register char *dst, register const char *src, size_t len );
EXTERN char* my_itoa( register char* str, long value, int radix );
EXTERN char *my_ftoa( register char *buf, double f );
EXTERN double my_round( double num, int prec );

#endif
