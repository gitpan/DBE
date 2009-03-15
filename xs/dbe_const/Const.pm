package DBE::Const;

# uncomment for debugging
#use strict;
#use warnings;

our ($VERSION, $ExportLevel);

BEGIN {
	$VERSION = '0.99_1';
	require XSLoader;
	XSLoader::load( __PACKAGE__, $VERSION );
	$ExportLevel = 0;
}

no warnings 'redefine';
1; # return

sub import {
	my $pkg = shift;
	if( $_[0] eq '-compile' ) {
		shift @_;
		&export( $pkg, @_ );
	}
	else {
		my $pkg_export = caller( $ExportLevel );
		&export( $pkg_export, @_ );
	}
}

sub compile {
	my $pkg_export = caller( $ExportLevel );
	&export( $pkg_export, @_ );
}

__END__

=head1 NAME

DBE::Const - Compile or Export SQL Constants


=head1 SYNOPSIS

  use DBE;
  use DBE::Const qw(SQL_DBMS_NAME);
  
  $con = DBE->connect( "..." );
  $dbms_name = $con->getinfo( SQL_DBMS_NAME );

=head1 DESCRIPTION

....

=head1 COPYRIGHT

The DBE::Const module is free software. You may distribute under the terms
of either the GNU General Public License or the Artistic License, as specified
in the Perl README file.

=cut
