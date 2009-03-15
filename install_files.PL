use File::Copy;

if( -d 'blib/arch/auto/DBE' ) {
	&copy( 'dbe.h', 'blib/arch/auto/DBE/dbe.h' );
	&copy( 'dbe_sql.h', 'blib/arch/auto/DBE/dbe_sql.h' );
}

1;
