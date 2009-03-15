#!perl

print "1..$_tests\n";

require DBE;
#import DBE;

$out = select( STDOUT );
$| = 0;
select( $out );

$con = DBE->connect( 'Provider=Text;Warn=0;Croak=0' );
_check( $con );

%drv = DBE->open_drivers();
_check( $drv{'TEXT'} );

_check( $con->close );

BEGIN {
	$_tests = 3;
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
