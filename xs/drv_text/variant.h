#ifndef __VARIANT_H__
#define __VARIANT_H__ 1

#include "common.h"

#define VAR_HAS_SV		1
#define VAR_HAS_IV		2
#define VAR_HAS_NV		4

typedef struct st_csv_var {
	union {
		long iv;
		double nv;
	};
	char *sv;
	size_t sv_len;
	unsigned char flags;
} csv_var_t;


#define VAR_FLAG_IV(v) \
	(v).flags = ((v).flags & (~(VAR_HAS_SV + VAR_HAS_NV))) | VAR_HAS_IV;

#define VAR_FLAG_NV(v) \
	(v).flags = ((v).flags & (~(VAR_HAS_SV + VAR_HAS_IV))) | VAR_HAS_NV;

#define VAR_FLAG_SV(v) \
	(v).flags = ((v).flags & (~(VAR_HAS_NV + VAR_HAS_IV))) | VAR_HAS_SV;


#define VAR_EVAL_NUM(x) { \
	if( ((x).flags & (VAR_HAS_IV + VAR_HAS_NV)) == 0 ) { \
		if( (x).sv != NULL ) { \
			if( strchr( (x).sv, '.' ) != NULL ) { \
				(x).nv = atof( (x).sv ); \
				(x).flags |= VAR_HAS_NV; \
			} \
			else { \
				(x).iv = atoi( (x).sv ); \
				(x).flags |= VAR_HAS_IV; \
			} \
		} \
		else { \
			(x).iv = 0; \
			(x).flags |= VAR_HAS_IV; \
		} \
	} \
}

#define VAR_EVAL_SV(x) { \
	if( (x).flags & VAR_HAS_IV ) { \
		Renew( (x).sv, sizeof( long ) / 2 * 5 + 2, char ); \
		(x).sv_len = my_itoa( (x).sv, (x).iv, 10 ) - (x).sv; \
		(x).flags |= VAR_HAS_SV; \
	} \
	else if( (x).flags & VAR_HAS_NV ) { \
		Renew( (x).sv, 64, char ); \
		(x).sv_len = my_ftoa( (x).sv, (x).nv ) - (x).sv; \
		(x).flags |= VAR_HAS_SV; \
	} \
	else if( (x).flags & VAR_HAS_SV ) { \
		if( (x).sv_len == 0 ) \
			(x).sv_len = strlen( (x).sv ); \
	} \
	else { \
		if( (x).sv != NULL ) { \
			Safefree( (x).sv ); \
		} \
		(x).sv_len = 0; \
	} \
}

#define VAR_EVAL_IV(x) { \
	if( ((x).flags & VAR_HAS_IV) == 0 ) { \
		if( (x).flags & VAR_HAS_NV ) { \
			(x).iv = (long) floor( (x).nv + 0.5 ); \
			(x).flags = ((x).flags & (~VAR_HAS_NV)) | VAR_HAS_IV; \
		} \
		else if( (x).sv != NULL ) { \
			(x).iv = atoi( (x).sv ); \
			(x).flags |= VAR_HAS_IV; \
		} \
		else { \
			(x).iv = 0; \
			(x).flags |= VAR_HAS_IV; \
		} \
	} \
}

#define VAR_EVAL_NV(x) { \
	if( ((x).flags & VAR_HAS_NV) == 0 ) { \
		if( (x).flags & VAR_HAS_IV ) { \
			(x).nv = (double) (x).iv; \
			(x).flags = ((x).flags & (~VAR_HAS_IV)) | VAR_HAS_NV; \
		} \
		else if( (x).sv != NULL ) { \
			(x).nv = atof( (x).sv ); \
			(x).flags |= VAR_HAS_NV; \
		} \
		else { \
			(x).nv = 0.0; \
			(x).flags |= VAR_HAS_NV; \
		} \
	} \
}

#define VAR_COPY(src,dst) { \
	if( (src).flags & VAR_HAS_IV ) { \
		(dst).iv = (src).iv; \
		VAR_FLAG_IV( (dst) ); \
	} \
	else if( (src).flags & VAR_HAS_NV ) { \
		(dst).nv = (src).nv; \
		VAR_FLAG_NV( (dst) ); \
	} \
	else if( (src).flags & VAR_HAS_SV ) { \
		if( (dst).sv_len < (src).sv_len ) { \
			Renew( (dst).sv, (src).sv_len + 1, char ); \
		} \
		Copy( (src).sv, (dst).sv, (src).sv_len + 1, char ); \
		(dst).sv_len = (src).sv_len; \
		VAR_FLAG_SV( (dst) ); \
	} \
	else { \
		(dst).flags = (dst).flags & (~(VAR_HAS_IV + VAR_HAS_NV + VAR_HAS_SV)); \
	} \
}

#define VAR_NUM_OP(x,a,b,OP,id) { \
	VAR_EVAL_NUM( (a) ) \
	VAR_EVAL_NUM( (b) ) \
	if( (a).flags & VAR_HAS_IV ) { \
		if( (b).flags & VAR_HAS_IV ) { \
			(x).iv = (a).iv OP (b).iv; \
			VAR_FLAG_IV( (x) ); \
		} \
		else { \
			(x).nv = (a).iv OP (b).nv; \
			VAR_FLAG_NV( (x) ); \
		} \
	} \
	else if( (b).flags & VAR_HAS_IV ) { \
		(x).nv = (a).nv OP (b).iv; \
		VAR_FLAG_NV( (x) ); \
	} \
	else { \
		(x).nv = (a).nv OP (b).nv; \
		VAR_FLAG_NV( (x) ); \
	} \
}

#define VAR_COMP_OP(x,a,b,OP1,id1,ne1,ne2) { \
	if( ! (a).flags ) { \
		(x).iv = (b).flags ? (ne2) : ! (ne2); \
	} \
	else if( ! (b).flags ) { \
		(x).iv = (ne1); \
	} \
	else if( (a).flags & VAR_HAS_NV ) { \
		VAR_EVAL_NV( (b) ); \
		(x).iv = (a).nv OP1 (b).nv; \
	} \
	else if( (b).flags & VAR_HAS_NV ) { \
		VAR_EVAL_NV( (a) ); \
		(x).iv = (a).nv OP1 (b).nv; \
	} \
	else if( (a).flags & VAR_HAS_IV ) { \
		VAR_EVAL_IV( (b) ); \
		(x).iv = (a).iv OP1 (b).iv; \
	} \
	else if( (b).flags & VAR_HAS_IV ) { \
		VAR_EVAL_IV( (a) ); \
		(x).iv = (a).iv OP1 (b).iv; \
	} \
	else { \
		if( (a).sv != NULL ) { \
			if( (b).sv != NULL ) \
				(x).iv = my_stricmp( (a).sv, (b).sv ) OP1 0; \
			else \
				(x).iv = (ne1); \
		} \
		else \
			(x).iv = (b).sv != NULL ? (ne2) : ! (ne2); \
	} \
	VAR_FLAG_IV( (x) ); \
}

#define VAR_SET_IV(x,v) { \
	(x).iv = (long) v; \
	VAR_FLAG_IV( (x) ); \
}

#define VAR_SET_NV(x,v) { \
	(x).nv = (double) v; \
	VAR_FLAG_NV( (x) ); \
}

#define VAR_SET_SV(x,v,l) { \
	size_t __l = l; \
	if( ! __l ) \
		__l = strlen( v ); \
	(x).sv_len = __l; \
	Renew( (x).sv, __l + 1, char ); \
	Copy( v, (x).sv, __l, char ); \
	(x).sv[__l] = '\0'; \
	VAR_FLAG_SV( (x) ); \
}

#endif
