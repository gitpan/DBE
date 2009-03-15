#ifndef __PARSE_H__
#define __PARSE_H__ 1

#include "parse_int.h"

extern void *ParseAlloc( void *(*mallocProc)(size_t) );
extern void ParseFree( void *p, void (*freeProc)(void*) );
extern void ParseTrace(FILE *TraceFILE, char *zTracePrompt);
extern void Parse( void *yyp, int yymajor, Token yyminor, csv_parse_t *parse );

EXTERN int run_parser( csv_parse_t *parse, const char *sql, size_t sql_len );

#endif
