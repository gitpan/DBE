package DBE::Text::Install;
use 5.006;
use ExtUtils::MakeMaker;
use Config;
# do only outside the DBE make process
if( ! $ENV{'MAKE_DBE'} ) {
	require DBE::Driver;
}

use strict;
our ($debug, %makeopts);

$debug = $ENV{'DBE_DEBUG'};
$Verbose = $ENV{'DBE_VERBOSE'} and print "\n--- Text Driver ---\n";

my $build = $ENV{'DBE_BUILD'};

my @argv;
foreach( @ARGV ) {
	$_ = lc( $_ );
	if( /--debug/ ) {
		if( m/--debug=(\d+)/ ) {
			$debug = $1;
		}
		else {
			$debug = 1;
		}
	}
	elsif( /--verbose/ ) {
		$Verbose = 1;
	}
	elsif( /--build/ ) {
		$build = 1;
	}
	else {
		push @argv, $_;
	}
}
@ARGV = @argv;


my %makeopts = (
	'NAME' => 'DBE::Driver::TEXT',
	'VERSION_FROM' => 'TEXT.pm',
	'ABSTRACT' => 'DBE Text Driver',
	'LIBS' => [],
	'DEFINE' => '',
	'INC' => ($ENV{'DBE_INC'} ? $ENV{'DBE_INC'} : '-I../../'),
	'XSPROTOARG' => '-noprototypes',
	'PREREQ_PM' => {
	},
	'OBJECT' => '$(O_FILES)',
	'XS' => { 'TEXT.xs' => 'TEXT.c' },
	'C' => [
		'qsort.c', 'common.c', 'inifile.c', 'charset.c',
		'csv_table.c', 'csv_select.c', 'csv_insert.c', 'csv_update.c',
		'csv_delete.c', 'csv_variables.c',
		'parse_fnc.c', 'parse_int.c', 'parse_tab.c', 'parse.c',
		'text_ext.c', 'dbe_text.c', 'TEXT.c'
	],
	'H' => [
		'common.h', 'inifile.h', 'charset.h', 'variant.h',
		'parse_int.h', 'parse_tab.h', 'parse.h',
		'text_ext.h', 'dbe_text.h'
	],
	'PL_FILES' => {},
);

if( $build ) {
	$makeopts{'PL_FILES'}{'drv_build.PL'} = 'true';
}

if( $debug ) {
	print "Text Driver: enable debug messages\n";
	$makeopts{'DEFINE'} .= ' -DCSV_DEBUG=' . $debug;
	if( $^O eq 'linux' ) {
		$makeopts{'DEFINE'} .= ' -Wall';
	}
}
if( $^O eq 'MSWin32' ) {
	$makeopts{'DEFINE'} .= ' -D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS';
	$makeopts{'LIBS'}[0] = '-lkernel32';
}
elsif( $^O eq 'cygwin' ) {
	$makeopts{'LIBS'}[0] = '-L/lib/w32api -lole32 -lversion';
}

WriteMakefile( %makeopts );

package MY;

sub cflags {
    my $inherited = shift->SUPER::cflags( @_ );
    if( $^O eq 'MSWin32' ) {
	    $inherited =~ s/-O1/-O2/sg;
    	# set static linking to crt
	    $inherited =~ s/-MD/-MT/sg;
	}
	if( ! $DBE::Text::Install::debug ) {
		$inherited =~ s/\s*-Wall\s*/ /sg;
	}
    $inherited;
}

sub const_loadlibs {
    my $inherited = shift->SUPER::const_loadlibs( @_ );
    if( $^O eq 'MSWin32' ) {
    	# set static linking to crt
	    $inherited =~ s/msvcrt\.lib/libcmt\.lib/sgi;
	}
    $inherited;
}
