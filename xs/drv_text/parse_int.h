#ifndef __PARSE_INT_H__
#define __PARSE_INT_H__ 1

#include "common.h"
#include "inifile.h"
#include "parse_tab.h"
#include "variant.h"
#include "charset.h"

#include <dbe.h>
/* use g_dbe to create regexp for the LIKE operation */
extern const dbe_t *g_dbe;

#ifdef _WIN32
	#define DIRSEP	'\\'
#else
	#define DIRSEP	'/'
#endif

#if ! defined CSV_DEBUG || CSV_DEBUG < 3
#define NDEBUG 1
#endif

#if 0 && defined(_WIN32)
extern void qsortG(
	void *base, size_t nmemb, size_t size,
	int (*compare)(const void *, const void *)
);
#undef qsort
#define qsort qsortG
#endif

typedef struct Token Token;
typedef struct Expr Expr;
typedef struct ExprList ExprList;
typedef struct st_csv_parse csv_parse_t;
typedef struct st_csv_select csv_select_t;
typedef struct st_csv_column_def csv_column_def_t;
typedef struct st_csv_table_def csv_table_def_t;
typedef struct st_csv_row csv_row_t;
typedef struct st_csv_result_set csv_result_set_t;
typedef struct st_csv_result_set csv_result_t;
typedef struct st_csv csv_t;
typedef struct st_csv_stmt csv_stmt_t;
typedef struct st_csv_insert csv_insert_t;
typedef struct st_csv_update csv_update_t;
typedef struct st_csv_delete csv_delete_t;

struct Token {
	const char *str;
	//int dyn;
	size_t len;
};

#define TK_ILLEGAL					0
#define TK_USER						1000
#define TK_ALL						TK_USER + 1
#define TK_SIGNED					TK_USER + 2
#define TK_UNSIGNED					TK_USER + 3
#define TK_CHAR						TK_USER + 4
#define TK_BINARY					TK_USER + 5
#define TK_PLAIN					TK_USER + 6

#define EXPR_IS_ID					1
#define EXPR_IS_ALL					2
#define EXPR_IS_AGGR				4
#define EXPR_USE_REF				8
#define EXPR_SORT_ASC				16
#define EXPR_IS_QUEST				32
#define EXPR_USE_PATTERN			64

struct Expr {
	csv_var_t var;
	int flags;
	int op;
	Expr *left, *right;
	ExprList *list;
	union {
		csv_column_def_t *column;
		csv_table_def_t *table;
		Expr *ref;
		csv_row_t *row;
		regexp *pat;
	};
	char *alias;
	size_t alias_len;
	Token *token;
};

struct ExprList {
	Expr **expr;
	size_t expr_count;
};

#define PARSE_TYPE_SELECT			1
#define PARSE_TYPE_CREATE_TABLE		2
#define PARSE_TYPE_DROP_TABLE		3
#define PARSE_TYPE_INSERT			4
#define PARSE_TYPE_UPDATE			5
#define PARSE_TYPE_DELETE			6
#define PARSE_TYPE_SET				7
#define PARSE_TYPE_SHOW_VARS		8

#define PARSE_CHECK_EXISTS			1

struct st_csv_parse {
	Token last_token;
	int has_aggr;
	Expr **qmark;
	size_t qmark_count;
	unsigned char type;
	unsigned char flags;
	union {
		csv_select_t *select;
		csv_table_def_t *table;
		csv_insert_t *insert;
		csv_update_t *update;
		csv_delete_t *delete;
		Expr *expr;
	};
	csv_t *csv;
	csv_stmt_t *stmt;
	char *sql;
	size_t sql_len;
};

#define CSV_ISALLOC				1
#define CSV_COLNAMEHEADER		2
#define CSV_USESCHEMA			4

#define BACKEND_CSV				1

struct st_csv {
	char						*path;
	size_t						path_length;
	unsigned char				backend;
	unsigned short				flags;
	char						delimiter;
	char						quote;
	char						decimal_symbol;
	size_t						max_scan_rows;
	enum n_charset				table_charset;
	enum n_charset				client_charset;
	size_t						affected_rows;
	short						last_errno;
	char						last_error[256];
};

struct st_csv_stmt {
	csv_t *csv;
	csv_parse_t parse;
	csv_result_t *result;
};

#define FIELD_TYPE_INTEGER		SQL_INTEGER
#define FIELD_TYPE_CHAR			SQL_CHAR
#define FIELD_TYPE_DOUBLE		SQL_DOUBLE
#define FIELD_TYPE_BLOB			SQL_BLOB

struct st_csv_column_def {
	char						*name;
	size_t						name_length;
	char						*typename;
	size_t						typename_length;
	int							type;
	size_t						size;
	char						*defval;
	size_t						defval_length;
	csv_table_def_t				*table;
	size_t						offset;
	size_t						length;
	char						*tablename;
	size_t						tablename_length;
};

#define DEFAULT_DELIMITER		';'
#define DEFAULT_QUOTECHAR		'"'
#define DEFAULT_MAXSCANROWS		25

#define TEXT_TABLE_COLNAMEHEADER			1
//#define TEXT_TABLE_QUOTEALL					2

#define COLUMN_COUNT_EXPAND					8

struct st_csv_table_def {
	char						*name;
	size_t						name_length;
	char						*alias;
	size_t						alias_length;
	csv_column_def_t			*columns;
	size_t						column_count;
	size_t						row_size;
	size_t						row_pos;
	PerlIO						*dstream;
	size_t						header_offset;
	char						*data;
	size_t						data_size;
	char						delimiter;
	char						quote;
	char						decimal_symbol;
	size_t						max_scan_rows;
	enum n_charset				charset;
	int							flags;
};

struct st_csv_select {
	csv_table_def_t				**tables;
	size_t						table_count;
	ExprList					*columns;
	Expr						*where;
	ExprList					*groupby;
	ExprList					*orderby;
	size_t						limit;
	size_t						offset;
};

struct st_csv_insert {
	csv_table_def_t				*table;
	Expr						**values;
};

struct st_csv_update {
	csv_table_def_t				*table;
	Expr						*where;
	Expr						**setlist;
};

struct st_csv_delete {
	csv_table_def_t				*table;
	Expr						*where;
};

struct st_csv_row {
	csv_var_t					*data;
	csv_var_t					*groupby;
	csv_var_t					*orderby;
	csv_select_t				*select;
};

#define ROW_COUNT_EXPAND		32

struct st_csv_result_set {
	csv_column_def_t			*columns;
	size_t						column_count;
	size_t						column_pos;
	csv_row_t					**rows;
	size_t						row_count;
	size_t						row_pos;
};

#define CSV_OK						0
#define CSV_ERROR					1
#define CSV_ERR_BASE				1000
#define CSV_ERR_SYNTAX				CSV_ERR_BASE + 1
#define CSV_ERR_STACKOVERFLOW		CSV_ERR_BASE + 2
#define CSV_ERR_UNKNOWNTABLE		CSV_ERR_BASE + 3
#define CSV_ERR_UNKNOWNCOLUMN		CSV_ERR_BASE + 4
#define CSV_ERR_BINDCOLUMN			CSV_ERR_BASE + 5
#define CSV_ERR_CREATETABLE			CSV_ERR_BASE + 6
#define CSV_ERR_DROPTABLE			CSV_ERR_BASE + 7
#define CSV_ERR_COLUMNCOUNT			CSV_ERR_BASE + 8
#define CSV_ERR_TMPFILE				CSV_ERR_BASE + 9
#define CSV_ERR_REMOVETABLE			CSV_ERR_BASE + 10
#define CSV_ERR_INVALIDPATH			CSV_ERR_BASE + 11
#define CSV_ERR_UNKNOWNCHARSET		CSV_ERR_BASE + 12
#define CSV_ERR_UNKNOWNVARIABLE		CSV_ERR_BASE + 13
#define CSV_ERR_UNKNOWNTYPE			CSV_ERR_BASE + 14


#define expr_token_len(a,t) { \
	if( (a)->token != NULL ) { \
		if( (a)->token->str > (t)->str ) { \
			(a)->token->len = (a)->token->str - (t)->str + (a)->token->len; \
			(a)->token->str = (t)->str; \
		} \
		else \
			(a)->token->len = (t)->str - (a)->token->str + (t)->len; \
	} \
}

EXTERN Expr *csv_expr( int op, Expr *a, Expr *b, ExprList *l, const Token *t );
EXTERN void csv_expr_free( Expr *x );
EXTERN int expr_eval( csv_parse_t *parse, Expr *x );

EXTERN ExprList *csv_exprlist_add( ExprList *l, Expr *a, const Token *alias );
EXTERN void csv_exprlist_free( ExprList *l );

#ifdef CSV_DEBUG
void print_expr( Expr *x, int level );
void print_exprlist( ExprList *l, int level );
#endif

void csv_begin_parse( csv_parse_t *parse );
/*
void csv_finish_coding( csv_parse_t *parse );
*/
void csv_error_msg( csv_parse_t *parse, int code, const char *msg, ... );
void csv_error_msg2( csv_t *csv, int code, const char *msg, ... );
int expr_bind_columns(
	csv_parse_t *parse, Expr *x, csv_table_def_t **tables, size_t table_count,
	const char *place
);
void csv_result_free( csv_result_t *result );

EXTERN csv_select_t *csv_select_new(
	csv_parse_t *parse, ExprList *fields, ExprList *tables, ExprList *joins,
	Expr *where, ExprList *groupby, ExprList *orderby,
	size_t limit, size_t offset
);
EXTERN void csv_select_free( csv_select_t *select );
EXTERN csv_result_set_t *csv_select_run( csv_parse_t *parse );
EXTERN void csv_select_start( csv_parse_t *parse, csv_select_t *select );

EXTERN int csv_create_table( csv_parse_t *parse );
EXTERN void csv_start_table( csv_parse_t *parse, Expr *name, int exists );
EXTERN void csv_table_add_column(
	csv_parse_t *parse, Expr *name, const char *typename,
	size_t typename_length, size_t size, ExprList *args
);
EXTERN void csv_start_drop_table( csv_parse_t *parse, Expr *name, int exists );
EXTERN int csv_drop_table( csv_parse_t *parse );

EXTERN int csv_start_insert(
	csv_parse_t *parse, Expr *name, ExprList *cols, ExprList *values
);
EXTERN int csv_insert_run( csv_parse_t *parse );
EXTERN void csv_insert_free( csv_insert_t *insert );

EXTERN void csv_start_update(
	csv_parse_t *parse, Expr *table, ExprList *setlist, Expr *where
);
EXTERN int csv_update_run( csv_parse_t *parse );
EXTERN void csv_update_free( csv_update_t *update );

EXTERN void csv_start_delete( csv_parse_t *parse, Expr *name, Expr *where );
EXTERN int csv_delete_run( csv_parse_t *parse );
EXTERN void csv_delete_free( csv_delete_t *delete );

EXTERN void csv_set_start( csv_parse_t *parse, Expr *k, Expr *v );
EXTERN int csv_set_run( csv_parse_t *parse );
EXTERN void csv_start_show_variables( csv_parse_t *parse, Expr *like );
EXTERN csv_result_set_t *csv_run_show_variables( csv_parse_t *parse );

EXTERN csv_table_def_t *csv_open_table(
	csv_parse_t *parse, const char *tablename, size_t tablename_length,
	const char *alias, size_t alias_length, const char *mode
);
EXTERN char *csv_read_row( csv_table_def_t *table );
EXTERN void csv_column_def_free( csv_column_def_t *column );
EXTERN csv_column_def_t *csv_column_clone( csv_column_def_t *column );
EXTERN void csv_table_def_free( csv_table_def_t *table );
EXTERN int get_field_type_by_name( const char *name );
EXTERN size_t get_column_size( int type, size_t size );
int csv_set_path( csv_t *csv, char *path, size_t path_len );

EXTERN void fnc_avg( Expr *x );
EXTERN void fnc_min( Expr *x );
EXTERN void fnc_max( Expr *x );
EXTERN void fnc_sum( Expr *x );
EXTERN void fnc_count( Expr *x );

EXTERN void fnc_abs( Expr *x );
EXTERN void fnc_round( Expr *x );

EXTERN void fnc_concat( Expr *x );
EXTERN void fnc_lower( Expr *x, enum n_charset cs );
EXTERN void fnc_upper( Expr *x, enum n_charset cs );
EXTERN void fnc_trim( Expr *x );
EXTERN void fnc_ltrim( Expr *x );
EXTERN void fnc_rtrim( Expr *x );
EXTERN void fnc_substr( Expr *x, enum n_charset cs );
EXTERN void fnc_octet_length( Expr *x, enum n_charset cs );
EXTERN void fnc_char_length( Expr *x );
EXTERN void fnc_locate( Expr *x, enum n_charset cs );
EXTERN void fnc_ascii( Expr *x );

EXTERN void fnc_current_timestamp( Expr *x );
EXTERN void fnc_current_time( Expr *x );
EXTERN void fnc_current_date( Expr *x );

EXTERN int fnc_convert( csv_parse_t *parse, Expr *x );

#define WRITE_QUOTED(stream,str,size,quote) \
	do { \
		register size_t __size = (size); \
		register char *__str = (str); \
		register char __ch = *__str ++; \
		PerlIO_putc( (stream), (quote) ); \
		do { \
			if( __ch == (quote) ) { \
				PerlIO_putc( (stream), (quote) ); \
				PerlIO_putc( (stream), (quote) ); \
			} \
			else \
				PerlIO_putc( (stream), __ch ); \
			__ch = *__str ++; \
		} while( -- __size > 0 ); \
		PerlIO_putc( (stream), (quote) ); \
	} while( 0 )


#endif
