#ifndef __INCLUDE_TEXT_EXT_H__
#define __INCLUDE_TEXT_EXT_H__ 1

#include "parse.h"

csv_t *csv_init( csv_t *csv );
void csv_free( csv_t *csv );
int csv_prepare(
	csv_t *csv, const char *sql, size_t sql_len, csv_stmt_t **pstmt );
int csv_execute( csv_stmt_t *stmt );
void csv_stmt_free( csv_stmt_t *stmt );
int csv_bind_param_sv(
	csv_stmt_t *stmt, WORD p_num, const char *val, size_t len );
int csv_bind_param_iv( csv_stmt_t *stmt, WORD p_num, int val );
int csv_bind_param_nv( csv_stmt_t *stmt, WORD p_num, double val );
csv_result_t *csv_get_result( csv_stmt_t *stmt );

#endif
