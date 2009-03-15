#!perl

print "1..$_tests\n";

require DBE;
#import DBE;

#DBE->trace( 2 );

$con = DBE->connect( 'Provider=Text;Warn=0;Croak=0' );

$table = 'test_table.txt';

$con->do( qq{DROP TABLE IF EXISTS "$table"} );
$con->do(
	qq{CREATE TABLE "$table" (
		ID int default 1111,
		Val varchar(50) default 'defval'
	)}
);
$con->do( qq{INSERT INTO "$table" VALUES(1,'test')} );
$con->do( qq{INSERT INTO "$table" (ID) VALUES(2)} );
$con->do( qq{INSERT INTO "$table" VALUES(3,NULL)} );

$row_max = 3;
$col_max = 2;

$res = $con->query( qq{SELECT * FROM "$table"} );
_check( $res );

@names = $res->fetch_names;
_check( scalar @names == $col_max );
_check( $names[0] eq 'ID' );
_check( $names[1] eq 'Val' );

_check( $res->num_fields == $col_max );
%fld = $res->fetch_field;
_check( $fld{'COLUMN_NAME'} eq 'ID' );
_check( $res->field_tell == 1 );
%fld = $res->fetch_field;
_check( $fld{'COLUMN_NAME'} eq 'Val' );
_check( $res->field_tell == 2 );
_check( $res->field_seek( 0 ) == 2 );

_check( $res->num_rows == $row_max );
_check( scalar $res->fetch_col() == $row_max );
_check( $res->row_tell == $row_max );
_check( $res->row_seek( 0 ) == $row_max );
_check( $res->fetch_col( \%col ) );
_check( $col{'1'} eq 'test' );
_check( $res->row_seek( 0 ) == $row_max );
_check( $res->fetch_col( \%col, 1, 0 ) );
_check( $col{'test'} eq '1' );
_check( $res->row_seek( 0 ) == $row_max );

@row = $res->fetch_row;
_check( scalar @row == $col_max );
_check( $row[0] == 1 );
_check( $row[1] eq 'test' );
_check( $res->row_tell == 1 );
_check( $res->row_seek( 0 ) == 1 );
_check( $res->fetch_row( \@row ) );
_check( scalar @row == $col_max );
_check( $row[0] == 1 );
_check( $row[1] eq 'test' );
$res->row_seek( 0 );
_check( $res->fetch_row( \%row ) );
_check( scalar keys %row == $col_max );
_check( $row{'ID'} == 1 );
_check( $row{'Val'} eq 'test' );
$res->row_seek( 0 );
$row = $res->fetchrow_arrayref();
_check( $row->[0] == 1 );
_check( $row->[1] eq 'test' );
$row = $res->fetchrow_hashref();
_check( $row->{'ID'} == 2 );
_check( $row->{'Val'} eq 'defval' );
$row = $res->fetchrow_hashref();
_check( $row->{'ID'} == 3 );
_check( ! defined $row->{'Val'} );
$res->row_seek( 0 );

$all = $res->fetchall_arrayref();
_check( scalar @$all == $row_max );
$res->row_seek( 0 );
$all = $res->fetchall_arrayref( {} );
_check( scalar @$all == $row_max );
_check( $all->[0]->{'ID'} == 1 );
$res->row_seek( 0 );
$all = $res->fetchall_hashref( 'Bla' );
_check( ! $all );
$all = $res->fetchall_hashref( 'ID' );
_check( $all->{'1'}->{'Val'} eq 'test' );
$res->row_seek( 0 );
$all = $res->fetchall_hashref( 'ID', 'Val' );
_check( $all->{'1'}->{'test'} );

$con->do( qq{DROP TABLE "$table"} );

undef $res;

BEGIN {
	$_tests = 45;
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

sub _fail_all {
	for( ; $_pos <= $_tests; $_pos ++ ) {
		print "fail $_pos\n";
	}
}
