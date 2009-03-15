#!perl

print "1..$_tests\n";

require DBE;
#import DBE;

#DBE->trace( 2 );

$out = select( STDOUT );
$| = 0;
select( $out );

$con = DBE->connect( 'Provider=Text;Warn=0;Croak=0' );

$table = 'test_table.txt';

$con->do( qq{DROP TABLE IF EXISTS "$table"} );
$con->do(
	qq{CREATE TABLE "$table" (
		ID int default 1111,
		Val varchar(50) default 'defval'
	)}
);
$con->do( qq{INSERT INTO "$table" VALUES(1,'test')} );
$con->do( qq{INSERT INTO "$table" (ID) VALUES(2)} );

$res = $con->tables();
_check( $res );
_check( $res->num_rows > 0 );

$res = $con->columns( '', '', $table );
_check( $res );
_check( $res->num_rows > 0 );
while( $row = $res->fetchrow_hashref ) {
	_check( $row->{'TABLE_NAME'} eq $table );
}

$con->do( qq{DROP TABLE "$table"} );

BEGIN {
	$_tests = 6;
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
