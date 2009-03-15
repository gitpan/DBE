#!perl

print "1..$_tests\n";

require DBE::Const;
DBE::Const::compile(
	qw/$SQL_DBMS_NAME SQL_DBMS_NAME/
);

_check( $SQL_DBMS_NAME );
_check( &SQL_DBMS_NAME );
_check( $SQL_DBMS_NAME == &SQL_DBMS_NAME );

DBE::Const::compile( qw/:sql_types/ );
_check( &SQL_INTEGER == 4 );

BEGIN {
	$_tests = 4;
	$_pos = 1;
	unshift @INC, 'blib/lib', 'blib/arch';
}

1;

sub _check {
	my( $val ) = @_;
	print "" . ($val ? "ok" : "fail") . " $_pos\n";
	$_pos ++;
}

sub _skip {
	my( $val ) = @_;
	print STDERR "Skipped: " . ($_[0] || "for various reasons") . "\n";
	print "" . ($val ? "ok" : "fail") . " $_pos\n";
	$_pos ++;
}

sub _skip_all {
	print STDERR "Skipped: " . ($_[0] || "for various reasons") . "\n";
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "ok $_pos\n";
	}
	exit;
}
