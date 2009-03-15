#ifndef __DBE_TEXT_H__
#define __DBE_TEXT_H__ 1

#include <dbe.h>
#include "text_ext.h"
#include "drv_build.h"

#define DRV_NAME "DBE Text Driver"

extern const dbe_t *g_dbe;
extern const drv_def_t g_drv_def;

struct st_drv_con {
	csv_t				csv;
	dbe_con_t			*pcon;
	dbe_str_t			*extensions;
	int					extensions_count;
	size_t				extensions_length;
};

struct st_drv_res {
	csv_result_t		*res;
	drv_con_t			*con;
	SV					**fields;
};

struct st_drv_stmt {
	csv_stmt_t			*stmt;
	drv_con_t			*con;
};

#endif
