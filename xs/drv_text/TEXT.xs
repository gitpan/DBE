#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "dbe_text.h"

MODULE = DBE::Driver::TEXT		PACKAGE = DBE::Driver::TEXT

BOOT:
{
#ifdef CSV_DEBUG
	_debug( "booting [%s]\n", g_drv_def.description );
#endif
}

void
init( dbe, ... )
	void *dbe;
PPCODE:
#if CSV_DEBUG > 1
	debug_init();
#endif
	g_dbe = (const dbe_t *) dbe;
	XSRETURN_IV( PTR2IV( &g_drv_def ) );
