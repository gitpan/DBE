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

$bin = $con->quote_bin( 'öäü' );
_check( $con->attr_set( 'charset' => 'ansi' ) );
$r = $con->do(
	qq/% INSERT INTO [_table1] ([name],[value]) VALUES('öäü',$bin)/ );
_check( $r );
_check( $con->attr_set( 'charset' => 'utf8' ) );
$r = $con->do(
	qq/% INSERT INTO [_table1] ([name],[value]) VALUES('öäü',$bin)/ );
_check( $r );
$res = $con->query( q/% SELECT * FROM [_table1]/ );
_check( $res );
_check( $res->num_rows >= 2 );
$res->row_seek( $res->num_rows - 2 );
$res->fetch_row( \%row1 );
$res->fetch_row( \%row2 );
_check( $row1{'name'} ne $row2{'name'} );
_check( $row1{'value'} eq $row2{'value'} );

BEGIN {
	$_tests = 8;
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
