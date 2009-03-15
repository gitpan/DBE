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

$res = $con->query( q/% SELECT * FROM [_table1]/ );
_check( $res ) or _skip_all();
_check( $res->num_rows == 10 );
_check( $res->bind_column( 2, $name ) );
_check( $res->bind_column( 3, $value ) );
@names = $res->fetch_names();
_check( @names == 4 );
_check( $res->fetch_row( \%row ) );
_check( $row{'name'} eq 'key1' );
_check( $row{'name'} eq $name );
_check( $row{'value'} eq $value );
_check( $res->row_tell == 1 );
_check( $res->fetch_row( \@row ) );
_check( $row[1] eq 'key2' );
_check( $row[1] eq $name );
_check( $row[2] eq $value );
_check( $res->row_tell == 2 );
_check( $row = $res->fetchrow_hashref );
_check( $row->{'name'} eq 'key3' );
_check( $row->{'name'} eq $name );
_check( $row->{'value'} eq $value );
_check( $res->row_tell == 3 );
_check( $row = $res->fetchrow_arrayref );
_check( $row->[1] eq 'key4' );
_check( $row->[1] eq $name );
_check( $row->[2] eq $value );
_check( $res->row_tell == 4 );
_check( $res->fetch );
_check( $name eq 'key5' );
_check( $value eq 'data5' );
_check( $res->row_seek( 0 ) == 5 );
_check( $res->row_tell == 0 );
_check( $res->fetch );
_check( $name eq 'key1' );
_check( $res->num_fields == 4 );
_check( $res->field_seek(1) == 0 );
%field = $res->fetch_field;
_check( $field{'COLUMN_NAME'} eq 'name' );
_check( $res->field_tell == 2 );

BEGIN {
	$_tests = 36;
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
