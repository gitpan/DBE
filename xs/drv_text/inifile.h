#ifndef __INCLUDE_INIFILE_H__
#define __INCLUDE_INIFILE_H__ 1

#include "common.h"

#ifndef PATH_MAX
#define PATH_MAX				512
#endif

typedef struct st_ini_item		ini_item_t;
typedef struct st_ini_section	ini_section_t;
typedef struct st_ini_file		ini_file_t;

struct st_ini_item {
	ini_item_t					*prev;
	ini_item_t					*next;
	char						*ident;
	size_t						ident_length;
	char						*value;
	size_t						value_length;
	char						*p_comment;
	size_t						p_comment_length;
};

struct st_ini_section {
	ini_section_t				*prev;
	ini_section_t				*next;
	char						*name;
	size_t						name_length;
	char						*s_comment;
	size_t						s_comment_length;
	char						*p_comment;
	size_t						p_comment_length;
	ini_item_t					*first_item;
	ini_item_t					*last_item;
};

struct st_ini_file {
	char						filename[PATH_MAX];
	ini_section_t				*first_section;
	ini_section_t				*last_section;
	char						*p_comment;
	size_t						p_comment_length;
	char						*newline;
	size_t						newline_length;
	char						changed;
};


ini_file_t *inifile_open( const char *filename );
void inifile_close( ini_file_t *ini );
int inifile_load( ini_file_t *ini );
int inifile_save( ini_file_t *ini );
const char *inifile_read_string(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	const char *s_default
);
void inifile_write_string(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	const char *s_value
);
int inifile_read_integer(
	ini_file_t *ini, const char *s_section, const char *s_ident, int i_default
);
void inifile_write_integer(
	ini_file_t *ini, const char *s_section, const char *s_ident, int i_value
);
double inifile_read_float(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	double d_default
);
void inifile_write_float(
	ini_file_t *ini, const char *s_section, const char *s_ident, double d_value
);
char inifile_read_char(
	ini_file_t *ini, const char *s_section, const char *s_ident,
	char c_default
);
void inifile_write_char(
	ini_file_t *ini, const char *s_section, const char *s_ident, char c_value
);
void inifile_delete_section( ini_file_t *ini, const char *s_section );
void inifile_delete_ident(
	ini_file_t *ini, const char *s_section, const char *s_ident );

#endif
