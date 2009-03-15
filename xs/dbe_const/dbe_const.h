#ifndef __DBE_CONST_H__
#define __DBE_CONST_H__ 1

#include <dbe.h>

typedef struct st_dbe_export_item {
	const char *name;
	long value;
} dbe_export_item_t;

typedef struct st_dbe_export_tag {
	const char *name;
	int item_start;
	int item_end;
} dbe_export_tag_t;

long dbe_const_by_name( const char *str, size_t str_len, int *empty );

extern const dbe_export_item_t dbe_export_items[];
extern const dbe_export_tag_t dbe_export_tags[];
extern const size_t dbe_export_tags_count;

#endif
