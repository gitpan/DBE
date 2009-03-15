#!perl

print "1..$_tests\n";

require DBE;
#import DBE;

$out = select( STDOUT );
$| = 0;
select( $out );

#DBE->trace( "SQL" );

$con = DBE->connect( 'Provider=Text;Warn=0;Croak=0' );
_check( $con );

$table = 'test_table.txt';

$r = $con->do( qq{DROP TABLE IF EXISTS "$table"} );
_check( $r );

# cause a syntax error
$r = $con->do( qq{CREATE TABLE} );
_check( ! $r );

$r = $con->do(
	qq{CREATE TABLE "$table" (
		ID int default 1111,
		Val varchar(50) default 'defval'
	)}
);
_check( $r );

$r = $con->do( qq{INSERT INTO "$table" VALUES(1,'test')} );
_check( $r );

$r = $con->do( qq{INSERT INTO "$table" (ID) VALUES(2)} );
_check( $r );

$r = $con->do( qq{INSERT INTO "$table" (Val) VALUES('line3')} );
_check( $r );

$r = $con->do( qq{INSERT INTO "$table" (ID,Val) VALUES(4,NULL)} );
_check( $r );
_check( $con->affected_rows == 1 );

$r = $con->query( qq{SELECT * FROM "$table" WHERE ID = 1} );
_check( $r );
_check( $r && $r->num_rows == 1 );
_check( $r && $r->fetch_row( $row = {} ) );
_check( $r && $row->{'ID'} == 1 );
_check( $r && $row->{'Val'} eq 'test' );

$r = $con->query( qq{SELECT * FROM "$table" WHERE ID = ?}, 2 );
_check( $r );
_check( $r && $r->num_rows == 1 );
_check( $r && $r->fetch_row( $row = {} ) );
_check( $r && $row->{'ID'} == 2 );
_check( $r && $row->{'Val'} eq 'defval' );

$r = $con->query( qq{SELECT * FROM "$table" WHERE Val LIKE ?}, '_ine3' );
_check( $r );
_check( $r && $r->num_rows == 1 );
_check( $r && $r->fetch_row( $row = {} ) );
_check( $r && $row->{'ID'} == 1111 );
_check( $r && $row->{'Val'} eq 'line3' );

$r = $con->query( qq{SELECT * FROM "$table" WHERE Val IS NULL} );
_check( $r );
_check( $r && $r->num_rows == 1 );
_check( $r && $r->fetch_row( $row = {} ) );
_check( $r && $row->{'ID'} == 4 );
_check( $r && ! defined $row->{'Val'} );

$r = $con->query( qq{SELECT COUNT(*) AS "Count" FROM "$table"} );
_check( $r );
_check( $r && $r->num_rows == 1 );
_check( $r && $r->fetch_row( $row = {} ) );
_check( $r && $row->{'Count'} == 4 );
_check( $r && $r->close );

$r = $con->do( qq{UPDATE "$table" SET Val = 'line4' WHERE ID = '4'} );
_check( $r );
_check( $con->affected_rows == 1 );

$r = $con->do( qq{DROP TABLE "$table"} );
_check( $r );

_check( $con->close );

BEGIN {
	$_tests = 38;
	$_pos = 1;
	unless( defined $ENV{'HARNESS_ACTIVE'} ) {
		unshift @INC, 'blib/lib', 'blib/arch';
	}
}

1;

sub _check {
	my( $val ) = @_;
	print "" . ($val ? "ok" : "not ok") . " $_pos\n";
	$_pos ++;
}

sub _skip_all {
	print STDERR "Skipped: " . ($_[0] || "various reasons") . "\n";
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "ok $_pos\n";
	}
}
