#!perl

print "1..$_tests\n";

require DBE;
_check( 1 );
import DBE;
_check( 1 );

BEGIN {
	$_tests = 2;
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
