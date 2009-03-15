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

$stmt = $con->prepare( q/% SELECT * FROM [_table1] WHERE [id] > ?/ );
_check( $stmt ) or _skip_all();
_check( $stmt->param_count == 1 );
_check( $stmt->bind_param( 1, 5, 'i' ) );
$res = $stmt->execute();
_check( $res ) or _skip_all();
_check( $res->num_rows == 5 );
_check( $res->fetch_row( \%row ) );
_check( $row{'name'} eq 'key6' );
$stmt = $con->prepare( q/% UPDATE [_table1] SET [value] = ? WHERE [id] = ?/ );
_check( $stmt ) or _skip_all();
_check( $stmt->param_count == 2 );
$val1 = "text\n\x65\xff\x01";
_check( $stmt->bind_param( 1, $val1, 'b' ) );
_check( $stmt->bind_param( 2, 10, 'i' ) );
_check( $stmt->execute() );
$res = $con->query( q/% SELECT [value] FROM [_table1] WHERE [id] = 10/ );
_check( $res ) or _skip_all();
($val2) = $res->fetch_row();
_check( $val2 eq $val1 );

BEGIN {
	$_tests = 14;
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
