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

$r = $con->do( q/% DROP TABLE IF EXISTS [_table1]/ );
_check( $r );
$r = $con->do(
	q/%
	CREATE TABLE [_table1] (
		[id] INTEGER,
		[name] VARCHAR(255),
		[value] BLOB,
		[time] TIMESTAMP
	)
	/
);
for $i( 1 .. 10 ) {
	$r = $con->do(
		qq/%
		INSERT INTO [_table1]
		([id], [name], [value])
		VALUES('$i', 'key$i', 'data$i')
		/
	);
	_check( $r );
	_check( $con->affected_rows == 1 );
}

BEGIN {
	$_tests = 21;
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
	print STDERR "Skipped: " . ($_[0] || "for various reasons") . "\n";
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "ok $_pos\n";
	}
	exit;
}
