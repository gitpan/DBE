#!perl

BEGIN {
	$_tests = 8;
	unless( defined $ENV{'HARNESS_ACTIVE'} ) {
		unshift @INC, 'blib/lib', 'blib/arch';
	}
	my $o = select STDOUT;
	$| = 1;
	select $o;
	print "1..$_tests\n";

	require Config;
	if( ! $Config::Config{'useithreads'} ) {
		print STDERR "Skip: not supported on this platform\n";
		for( $_pos = 1; $_pos <= $_tests; $_pos ++ ) {
			print "ok $_pos\n";
		}
		exit( 0 );
	}

	$SIG{'__DIE__'} = sub {
		print STDERR "Skip: not supported on this platform\n";
		for( $_pos = 1; $_pos <= $_tests; $_pos ++ ) {
			print "ok $_pos\n";
		}
		exit( 0 );
	};

}

use threads;
use threads::shared;

$SIG{'__DIE__'} = '';

require DBE;

#DBE->trace( 2 );

our $RUNNING : shared = 1;
our $_pos : shared = 1;

$DSN = 'Provider=Text;Warn=1;Croak=0';
$DB = DBE->connect( $DSN );
_check( $DB );

for my $i( 1 .. 3 ) {
	threads->create( \&thread, $i );
}

foreach $thread( threads->list ) {
	eval {
		$thread->join();
	};
}

_check( $DB->query( 'SELECT 1' ) );

1;

sub thread {
	my( $res, $db );
	# global $DB is not blessed here!
	$db = DBE->connect( $DSN );
	_check( $db );
	_check( $db && $db->query( 'SELECT 1' ) );
}

sub _check {
	my( $val ) = @_;
	lock( $_pos );
	print "" . ($val ? "ok" : "not ok") . " $_pos\n";
	$_pos ++;
}

sub _skip_all {
	print STDERR "Skipped: " . ($_[0] || "various reasons") . "\n";
	lock( $_pos );
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "ok $_pos\n";
	}
}
