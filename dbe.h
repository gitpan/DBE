#ifndef __DBE_H__
#define __DBE_H__ 1

#ifdef __cplusplus
extern "C" {
#endif

/* Include file for DBE drivers. */

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#ifndef SQL_SUCCESS
#include <dbe_sql.h>
#endif

/* installed version of DBE */
#define DBE_VERSION				1000
#define DBE_VERSION_STRING		"1.0"

/* DBE return codes */
#define DBE_OK				  0
#define DBE_ERROR			(-1)
#define DBE_CONN_LOST		(-2)
#define DBE_NO_DATA			(-3)
#define DBE_INVALIDARG		(-4)

/* DBE flags */
#define DBE_FREE_ARRAY		0x01
#define DBE_FREE_ITEMS		0x02

/* DBE trace level */
#define DBE_TRACE_NONE		0
#define DBE_TRACE_SQL		1
#define DBE_TRACE_SQLFULL	2
#define DBE_TRACE_ALL		3

/* definition of long long */
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 xlong;
typedef unsigned __int64 u_xlong;
#else
typedef long long int xlong;
typedef unsigned long long int u_xlong;
#endif

/* types in virtual result sets */
enum dbe_type {
	DBE_TYPE_VARCHAR	=	 12,		/*   (char *)    */
	DBE_TYPE_INTEGER	=	  4,		/*   (int)       */
	DBE_TYPE_DOUBLE		=	  8,		/*   (double)    */
	DBE_TYPE_BINARY		=	(-2),		/*   (void *)    */
	DBE_TYPE_BIGINT		=	(-5)		/*   (xlong)     */
};

/* dbe string */
typedef struct st_dbe_str {
	char *value;
	size_t length;
} dbe_str_t;

/* dbe object codes */
enum dbe_object {
	DBE_OBJECT_CON = 1,
	DBE_OBJECT_RES = 2,
	DBE_OBJECT_STMT = 3,
	DBE_OBJECT_LOB = 4,
};

/* dbe structure */
typedef struct st_dbe				dbe_t;

/* driver definition structure */
typedef struct st_drv_def			drv_def_t;

/* dbe internal types */
typedef struct st_dbe_drv			dbe_drv_t;
typedef struct st_dbe_con			dbe_con_t;
typedef struct st_dbe_res			dbe_res_t;

/* dbe drv prototypes */
typedef struct st_drv_con			drv_con_t;
typedef struct st_drv_res			drv_res_t;
typedef struct st_drv_stmt			drv_stmt_t;


/* dbe structure */
struct st_dbe {

	/* DBE version */
	const int version;

	/* sets an error code and message to the dbe connection handle. */
	void (*set_error) ( dbe_con_t *dbe_con, int code, const char *msg, ... );
	
	/* trace messages
	   - set dbe_con to NULL to make a global trace */
	void (*trace) (
		dbe_con_t *dbe_con, int level, int show_caller, const char *msg, ...
	);
	
	/* overload the default base class of connection, result or
	   statement objects to implement own functions
	   - base classes are DBE::CON, DBE::RES and DBE::STMT */
	void (*set_base_class) (
		dbe_drv_t *dbe_drv, enum dbe_object obj, const char *classname
	);
	
	/* retrieves a pointer to a driver object.
	   can be used in self defined class functions */
	void *(*get_object) ( enum dbe_object obj, SV *this );
	
	/* starts the error handler from your XS code
	   'obj' defines the type of sv 'this'
	   'action' holds a short description of the action that causes the error */
	void (*handle_error_xs) (
		dbe_con_t *dbe_con, enum dbe_object obj, SV *this,
		const char *action
	);
	
	/* fills the error message of a system error into 'buf'.
	   returns the pointer to 'buf' on success or NULL on error */
	char *(*error_str) ( int errnum, char *buf, size_t buflen );
	
/* driver result sets */

	/* allocates a dbe result set */
	dbe_res_t *(*alloc_result) (
		dbe_con_t *dbe_con, drv_res_t *drv_res, u_long num_fields );
	
	/* frees a dbe result set that was allocated by alloc_result() */
	void (*free_result) ( dbe_res_t *dbe_res );

/* virtual result sets */

	/* creates a virtual result set and returns a pointer to it */
	dbe_res_t *(*vres_create) ( dbe_con_t *dbe_con );

	/* frees a virtual result set */
	void (*vres_free) ( dbe_res_t *dbe_res );

	/* adds a field/column to a virtual result set */
	int (*vres_add_column) (
		dbe_res_t *dbe_res, const char *name, size_t name_length,
		enum dbe_type type
	);

	/* adds a row to a virtual result set. the data in the value part must
	   be conform to the column types:
	   - type DBE_TYPE_INTEGER points to int or NULL in 'data[x].value'
	   - type DBE_TYPE_DOUBLE points to double or NULL in 'data[x].value'
	   - type DBE_TYPE_BIGINT points to xlong or NULL in 'data[x].value'
	   - other types point to char or NULL */
	int (*vres_add_row) ( dbe_res_t *dbe_res, dbe_str_t *data );

/* regular expressions and search patterns */
	
	/* creates perl regexp from a string and returns it
	   i.e. /tab_.+/i */
	regexp *(*regexp_from_string) ( const char *str, size_t str_len );
	
	/* converts a SQL search pattern to a compatbile perl regexp
	   (i.e. "%FOO\_BAR_" to ".*?FOO_BAR.")
	   returns NULL when no placeholders has been found and
	   'force' was set to FALSE */
	regexp *(*regexp_from_pattern) (
		const char *pattern, size_t pattern_len, int force
	);
	
	/* counts the matches of a regular expression on 'str' and
	   returns the number of hits */
	int (*regexp_match) ( regexp *re, const char *str, size_t str_len );
	
	/* frees a perl regexp */
	void (*regexp_free) ( regexp *re );
	
	/* - unescapes a search pattern (i.e. table\_name to table_name)
	     within the string an returns the new size
	   - 'esc' defines the escape character */
	size_t (*unescape_pattern) ( char *str, size_t str_len, const char esc );
	
	/* - escapes a search pattern (i.e table_name to table\_name)
	     and returns a new string as SV
	   - 'esc' defines the escape character */
	SV *(*escape_pattern) (
		const char *str, size_t str_len, const char esc );
	
/* unicode functions */

	/* converts an unicode character (widechar/utf16be) to upper case */
	unsigned int (*towupper) ( unsigned int c );

	/* converts an unicode character (widechar/utf16be) to lower case */
	unsigned int (*towlower) ( unsigned int c );
	
	/* converts an utf-8 string from 'src' to unicode
	   and writes it into 'dst'
	   - returns the number of bytes written or -1 on invalid utf-8 sequence
	   - it is safe to make 'dst' twice bigger then 'src' */
	int (*utf8_to_unicode) (
		const char *src, size_t src_len, char *dst, size_t dst_max );
	
	/* converts an unicode string from 'src' to utf-8
	   and writes it into 'dst'
	   - returns the length of 'dst' without the \0 terminating character
	     or -1 on invalid unicode seq
	   - 'dst' could be up to two times longer then 'src' */
	int (*unicode_to_utf8) (
		const char *src, size_t src_len, char *dst, size_t dst_max );
	
	/* converts an utf-8 string from 'src' to upper case
	   and writes it into 'dst'
	   - returns the length of 'dst' without the \0 terminating character
	     or -1 on invalid utf-8 sequence
	   - it is safe to make 'dst' twice bigger then 'src' */
	int (*utf8_toupper) (
		const char *src, size_t src_len, char *dst, size_t dst_max );

	/* converts an utf-8 string from 'src' to lower case
	   and writes it into 'dst'
	   - returns the length of 'dst' without the \0 terminating character
	     or -1 on invalid utf-8 sequence
	   - it is safe to make 'dst' twice bigger then 'src' */
	int (*utf8_tolower) (
		const char *src, size_t src_len, char *dst, size_t dst_max );

/* lob functions */

	/* allocates a DBE::LOB class */
	SV *(*lob_stream_alloc) ( dbe_con_t *dbe_con, void *context );

	/* frees the lob stream and the destroys the class */
	void (*lob_stream_free) ( dbe_con_t *dbe_con, void *context );
	
	/* returns the context of the lob stream class */
	void *(*lob_stream_get) ( dbe_con_t *dbe_con, SV *sv );
};


/* getinfo types */
enum dbe_getinfo_type {
	DBE_GETINFO_STRING = 1,
	DBE_GETINFO_SHORTINT,		/* 16-bit */
	DBE_GETINFO_INTEGER,		/* 32-bit */
	DBE_GETINFO_32BITMASK,
};


/* getinfo return values */

/* type is not supported */
#define DBE_GETINFO_INVALIDARG		DBE_INVALIDARG
/* given size would truncate the result */
#define DBE_GETINFO_TRUNCATE		(-5)


/* driver definition structure */

struct st_drv_def {

/* header */

	/* must be set to DBE_VERSION */
	const int version;

	/* driver description */
	const char *description;
	
	/* frees memory that was allocated by the driver */
	void (*mem_free) ( void *ptr );

/* driver */

	/* can be used to cleanup the driver */
	void (*drv_free) ( drv_def_t *drv_def );

	/* makes a connection to the driver and returns a pointer to
	   the drivers connection object
	   - 'dbe_con' contains the DBE connection object which must be hold
	     by the driver to interact with DBE */
	drv_con_t *(*drv_connect) ( dbe_con_t *dbe_con, char **args, int argc );
	
/* connection */

	/* closes the connection and frees its resources */
	void (*con_free) ( drv_con_t *drv_con );

	/* determines the connection status
	   - returns DBE_OK, DBE_CONN_LOST or DBE_ERROR */
	int (*con_ping) ( drv_con_t *drv_con );

	/* resets the connection an restores previous settings */
	int (*con_reconnect) ( drv_con_t *drv_con );

	/* sets the trace level to the connection object */
	void (*con_set_trace) ( drv_con_t *drv_con, int level );
	
	/* sets a connection attribute
	   - returns DBE_INVALIDARG on invalid attribute name */
	int (*con_set_attr) ( drv_con_t *drv_con, const char *name, SV *value );
	
	/* reads a connection attribute
	   - the value must be set to *rvalue
	   - returns DBE_INVALIDARG on invalid attribute name */
	int (*con_get_attr) ( drv_con_t *drv_con, const char *name, SV **rvalue );

	/* sets a row limit and offset, enables memory friendly streaming */
	int (*con_row_limit) ( drv_con_t *drv_con, u_xlong limit, u_xlong offset );
	
	/* performs a simple query
	   - the result set must be created with alloc_result() or vres_create().
	     see 'structure st_dbe' for details */
	int (*con_query) (
		drv_con_t *drv_con, const char *sql, size_t sql_len,
		dbe_res_t **dbe_res
	);

	/* gets the auto generated id of the last statement */
	int (*con_insert_id) (
		drv_con_t *drv_con, const char *catalog, size_t catalog_len,
		const char *schema, size_t schema_len, const char *table,
		size_t table_len, const char *field, size_t field_len,
		u_xlong *id
	);

	/* returns the affected rows of the last statement */
	xlong (*con_affected_rows) ( drv_con_t *drv_con );

/* transactions */

	/* FALSE value in 'mode' turns auto commit off, TRUE turns it on */
	int (*con_auto_commit) ( drv_con_t *drv_con, int mode );
	
	/* starts a transaction */
	int (*con_begin_work) ( drv_con_t *drv_con );
	
	/* commits a transaction */
	int (*con_commit) ( drv_con_t *drv_con );
	
	/* cancels a transaction */
	int (*con_rollback) ( drv_con_t *drv_con );

/* statements */

	/* prepares a sql statement */
	int (*con_prepare) (
		drv_con_t *drv_con, const char *sql, size_t sql_len,
		drv_stmt_t **drv_stmt
	);
	
	/* frees a statement */
	void (*stmt_free) ( drv_stmt_t *drv_stmt );
	
	/* returns the number of parameters in the statement */
	int (*stmt_param_count) ( drv_stmt_t *drv_stmt );
	
	/* binds a value to a parameter in a prepared statement */
	int (*stmt_bind_param) (
		drv_stmt_t *drv_stmt, u_long p_num, SV *p_val, char p_type
	);
	
	/* returns the position of a parameter name */
	int (*stmt_param_position) (
		drv_stmt_t *drv_stmt, const char *name, size_t name_len,
		u_long *ppos
	);
	
	/* executes a prepared statement
	   - the result set must be ceated with alloc_result() or vres_create()
	     see 'structure st_dbe' for details */
	int (*stmt_execute) ( drv_stmt_t *drv_stmt, dbe_res_t **dbe_res );

/* result sets */

	void (*res_free) ( drv_res_t *drv_res );

	/* returns the number of rows in the result set, or -1 if its unknown */
	xlong (*res_num_rows) ( drv_res_t *drv_res );
	
	/* the 'names' array is allocated by DBE
	   - flag DBE_FREE_ITEMS tells DBE to free the 'value' part in the
	     'names' array */
	int (*res_fetch_names) (
		drv_res_t *drv_res, dbe_str_t *names, int *flags );

	/* - the 'row' array is allocated by DBE
	   - the values in 'row' must be set without sv_2mortal, undefined values
	     can point to NULL but not to &Pl_sv_undef
	   - 'names' indicates a fetching process with names included
	     it can be used to make hashes in the SVs of the 'row' if the
	     resulting fields contain structures (can be ignored in most cases)
	   - returns DBE_NO_DATA if no row becomes available anymore */
	int (*res_fetch_row) ( drv_res_t *drv_res, SV **row, int names );

	/* sets the row cursor to the offset and returns the previous offset
	   may not work with all drivers */
	xlong (*res_row_seek) ( drv_res_t *drv_res, u_xlong offset );
	
	/* returns the offset of the current row */
	xlong (*res_row_tell) ( drv_res_t *drv_res );
	
	/* - the '**hash' array must be allocated by the driver, it contains
	     an even number of elements in key-value style.
	   - flag DBE_FREE_ARRAY tells DBE to free the '***hash' parameter
	   - the values must be set without sv_2mortal; Undefined values should
	     point to NULL instead of &Pl_sv_undef
	   - returns the total number of elements (must be always even) */
	int (*res_fetch_field) (
		drv_res_t *drv_res, SV ***hash, int *flags
	);
	
	/* sets the column cursor to offset
	   returns the previous cursor position */
	u_long (*res_field_seek) ( drv_res_t *drv_res, u_long offset );
	
	/* returns the current column cursor position, starting at 0 */
	u_long (*res_field_tell) ( drv_res_t *drv_res );

/* lob functions */

	/* returns the size of the lob in bytes or characters,
	   or DBE_NO_DATA if it is unknown, or DBE_ERROR on error */
	int (*con_lob_size) ( drv_con_t *drv_con, void *context );

	/* reads 'length' bytes/characters from the lob starting at 'offset'
	   - the new offset must be set by the driver
	   - returns the number of bytes read, or DBE_NO_DATA on eof,
	     or DBE_ERROR on error */
	int (*con_lob_read) (
		drv_con_t *drv_con, void *context, int *offset, char *buffer, int length
	);
	
	/* write 'length' bytes/characters to the lob starting at 'offset'
	   - the new offset must be set by the driver
	   - returns the number of bytes written, or DBE_ERROR on error */
	int (*con_lob_write) (
		drv_con_t *drv_con, void *context, int *offset, char *buffer, int length
	);
	
	/* closes the lob stream.
	   returns DBE_OK on success, or DBE_ERROR on failure */
	int (*con_lob_close) ( drv_con_t *drv_con, void *context );

/* misc functions */
	
	/* quote string values (eg. [don't clash] as ['don''t clash']) */
	SV *(*con_quote) ( drv_con_t *drv_con, const char *str, size_t str_len );
	
	/* quote binary values (eg. as hex values) */
	SV *(*con_quote_bin) (
		drv_con_t *drv_con, const char *bin, size_t bin_len
	);
	
	/* quote identifier/s (eg. ("table", "column") to '"table"."column"' */
	SV *(*con_quote_id) ( drv_con_t *drv_con, const char **args, int argc );

/* information */
	
	/* get general information
	   - 'cbvalmax' has a minimum size of 256 bytes. you don't need to check
	     values which are never exceed the minimum */
	int (*con_getinfo) (
		drv_con_t *drv_con, int type, void *val, int cbvalmax, int *pcbval,
		enum dbe_getinfo_type *rtype
	);

	/* get information about the data types that are supported by the DBMS */
	int (*con_type_info) (
		drv_con_t *drv_con, int sql_type, dbe_res_t **dbe_res
	);

	/* - the 'names' array must be allocated by the driver, it contains
	     a list of names of data sources.
	   - the number of data sources must be set to 'count'
	   - 'wild' may contain wildcard characters like '_' or '%' */
	int (*con_data_sources) (
		drv_con_t *drv_con, char *wild, size_t wild_len,
		SV ***names, int *count
	);
	
	/* gets table information */
	int (*con_tables) (
		drv_con_t *drv_con, const char *catalog, size_t catalog_len,
		const char *schema, size_t schema_len, const char *table,
		size_t table_len, const char *type, size_t type_len,
		dbe_res_t **dbe_res
	);
	
	/* gets column information about a table */
	int (*con_columns) (
		drv_con_t *drv_con, const char *catalog, size_t catalog_len,
		const char *schema, size_t schema_len, const char *table,
		size_t table_len, const char *column, size_t column_len,
		dbe_res_t **dbe_res
	);
	
	/* gets primary key information for a specified table */
	int (*con_primary_keys) (
		drv_con_t *drv_con, const char *catalog, size_t catalog_len,
		const char *schema, size_t schema_len, const char *table,
		size_t table_len, dbe_res_t **dbe_res
	);
	
	int (*con_foreign_keys) (
		drv_con_t *drv_con, const char *pk_cat, size_t pk_cat_len,
		const char *pk_schem, size_t pk_schem_len, const char *pk_table,
		size_t pk_table_len, const char *fk_cat, size_t fk_cat_len,
		const char *fk_schem, size_t fk_schem_len, const char *fk_table,
		size_t fk_table_len, dbe_res_t **dbe_res
	);
	
	/* retrieves index information for a given table */
	int (*con_statistics) (
		drv_con_t *drv_con, const char *catalog, size_t catalog_len,
		const char *schema, size_t schema_len, const char *table,
		size_t table_len, int unique_only, dbe_res_t **dbe_res
	);
	
	int (*con_special_columns) (
		drv_con_t *con, int identifier_type, const char *catalog,
		size_t catalog_len, const char *schema, size_t schema_len,
		const char *table, size_t table_len, int scope, int nullable,
		dbe_res_t **dbe_res
	);

};

#ifdef __cplusplus
}
#endif

#endif  /* __DBE_H__ */
