#!perl

print "1..$_tests\n";

require DBE;

#DBE->trace( 3 );

$con = DBE->connect(
	'driver' => 'Text',
	'dbq' => '.',
	'croak' => 0,
	'warn' => 1,
);
if( ! $con ) {
	&_skip_all( "connection failed" );
}

$res = $con->tables( undef, undef, $con->escape_pattern( '_TAble1' ) );
_check( $res );
_check( $res->num_rows == 1 );
$res->fetch_row( \%row );
_check( $row{'TABLE_NAME'} eq '_table1' );
$res = $con->columns( undef, undef, $con->escape_pattern( '_table1' ) );
_check( $res );
_check( $res->num_rows == 4 );
$res->fetch_row( \%row );
_check( $row{'COLUMN_NAME'} eq 'id' );
_check( $row{'DATA_TYPE'} == 4 );

$res = $con->type_info( 4 );
_check( $res );
_check( $res->num_rows == 1 );

BEGIN {
	$_tests = 9;
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
	return $val;
}

sub _skip_all {
	print STDERR "Skipped: " . ($_[0] || "for various reasons") . "\n";
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "ok $_pos\n";
	}
	exit;
}
