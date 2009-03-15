package DBE::CompatDBI;
# =============================================================================
# DBE::CompatDBI - Emulate DBI with DBE
# =============================================================================

# uncomment for debugging
#use strict;
#use warnings;

our ($VERSION, %EXPORT_TAGS, @EXPORT_OK);

BEGIN {
	$VERSION = '1.41'; # set to compatible DBI version
	require DBE;
	require Carp;
	require Exporter;
	#*import = \&Exporter::import;
	my @export_utils = qw/
		neat neat_list $neat_maxlen data_string_desc looks_like_number/;
	%EXPORT_TAGS = (
		'utils' => \@export_utils,
	);
	@EXPORT_OK = @export_utils;
	*neat = \&DBI::neat;
	*neat_list = \&DBI::neat_list;
	*data_string_desc = \&DBI::data_string_desc;
	*looks_like_number = \&DBI::looks_like_number;
	$Carp::Internal{'DBE::CompatDBI'} ++;
}

no warnings 'redefine';

sub DBE::__caller__ {
	my $level = shift;
	my( @c );
	while( @c = CORE::caller( $level ++ ) ) {
		$Carp::Internal{$c[0]} or last;
	}
	return reverse @c;
}

sub import {
	my $pkg = shift;
	my @export = ();
	foreach( @_ ) {
		my $c = substr( $_, 0, 1 );
		if( $c eq ':' ) {
			my $k = substr( $_, 1 );
			if( $k eq 'sql_types' ) {
				my $caller_pkg = caller;
				$DBE::Const::VERSION or require DBE::Const;
				&DBE::Const::export( $caller_pkg, ':sql_types' );
				next;
			}
		}
		push @export, $_;
	}
	if( @export ) {
		$Exporter::ExportLevel ++;
		&Exporter::import( $pkg, @export );
		$Exporter::ExportLevel --;
	}
}

1;

################################ DBI ################################

package
	DBI;

our ($VERSION, $neat_maxlen, $TraceLevel, $err, $errstr, $state, $rows);

BEGIN {
	$VERSION = $DBE::CompatDBI::VERSION;
	$Carp::Internal{'DBI'} ++;
	no bytes;
	use utf8;
	$neat_maxlen = 400;
	*connect_cached = \&connect;
}

1;

sub connect {
	my ($pkg, $dsn, $user, $auth, $attr) = @_;
	my ($this, $con, $driver);
	# dbi:DriverName:database=database_name;host=hostname;port=port
	$user =~ s/\"/\/\"/gso;
	$auth =~ s/\"/\/\"/gso;
	if( $dsn !~ m/^\s*dbi\:(\w+)\:(.+)/i ) {
		&Carp::croak( "Invalid dsn $dsn" );
	}
	$driver = uc( $1 );
	if( $driver eq 'CSV' || $driver eq 'FILE' ) {
		$driver = 'TEXT';
	}
	$dsn = index( $2, '=' ) < 0 ? "database=\"$2\"" : $2;
	$dsn = "driver=$driver;user=\"$user\";auth=\"$auth\";$dsn";
	$this = {
		'AutoCommit' => 1,
		'PrintWarn' => $^W ? 1 : 0,
		'PrintError' => 1,
		'RaiseError' => 0,
		'Type' => 'db',
		'FetchHashKeyName' => 'NAME',
		'Driver' => {
			'Name' => $driver,
		},
		'Username' => $user,
	};
	if( $attr && ref( $attr ) eq 'HASH' ) {
		if( defined $attr->{'PrintError'} ) {
			$dsn .= ';warn=' . $attr->{'PrintError'};
			$this->{'PrintError'} = $attr->{'PrintError'};
		}
		if( defined $attr->{'RaiseError'} ) {
			$dsn .= ';croak=' . $attr->{'RaiseError'};
			$this->{'RaiseError'} = $attr->{'RaiseError'};
		}
		if( defined $attr->{'AutoCommit'} ) {
			$dsn .= ';autocommit=' . $attr->{'AutoCommit'};
			$this->{'AutoCommit'} = $attr->{'AutoCommit'};
		}
	}
	$con = DBE->connect( $dsn ) or return undef;
	$this->{'__con'} = $con;
	$this->{'__dsn'} = $dsn;
	$this->{'__autocommit'} = $this->{'AutoCommit'};
	$con->set_error_handler( \&_error_handler, $this );
	bless $this, 'DBI::db';
}

sub available_drivers {
	my %drv = DBE->installed_drivers();
	return sort values %drv;
}

sub installed_drivers {
	my %drv = DBE->open_drivers();
	return sort values %drv;
}

sub installed_versions {
	DBE->installed_drivers();
}

sub data_sources {
	();
}

sub errstr {
	$errstr;
}

sub err {
	$err;
}

sub _warn {
	my $this = shift;
	$err = -1;
	$errstr = ref( $this ) . ' ' . join( ' ', @_ );
	my $sub = $this->{'HandleError'};
	if( $sub && ref( $sub ) eq 'CODE' ) {
		$Carp::CarpLevel = 1;
		return $sub->( $errstr, $this );
		$Carp::CarpLevel = 0;
	}
	else {
		$this->{'PrintWarn'} and &Carp::carp( $errstr );
	}
	return undef;
}

sub _error_handler {
	my ($this, $code, $msg, $provider, $action, $obj_id, $obj) = @_;
	&set_err( $this, $code, "[$provider] $action: $msg" );
}

sub set_err {
	my ($this, $err, $errstr, $state, $method, $rv) = @_;
	$DBI::err = $err;
	$DBI::errstr = $errstr;
	$DBI::state = $state;
	$this->{'__err'} = $err;
	$this->{'__errstr'} = $errstr;
	$this->{'__state'} = $state;
	if( $err ) {
		my $sub = $this->{'HandleError'};
		if( $sub && ref( $sub ) eq 'CODE' ) {
			$Carp::CarpLevel = 1;
			$sub->( $errstr, $this );
			$Carp::CarpLevel = 0;
		}
		else {
			$this->{'PrintError'} and &Carp::carp( $errstr );
			$this->{'RaiseError'} and &Carp::croak( $errstr );
		}
	}
	return $rv;
}

sub trace {
	my( $pkg, $level, $fh ) = @_;
	$level or return $TraceLevel;
	if( $fh && -f $fh ) {
		open $fh, ">> $fh" or &Carp::croak( "cant open file: $!" );
	}
	$TraceLevel = $level;
	my @level = split( /[\|,]/, $level );
	$level[0] = int( $level[0] ) or return DBE->trace( 0 );
	if( uc( $level[1] ) eq 'SQL' ) {
		$level[0] = 2;
	}
	else {
		$level[0] = 3;
	}
	DBE->trace( $level[0], $fh );
}

sub data_string_desc {
	my( $str ) = @_;
	my $v = utf8::valid( $str );
	$v and utf8::decode( $str );
	my $res = 'UTF8 ' . (utf8::is_utf8( $str ) ? 'ON' : 'OFF');
	$v or $res .= ' but invalid encoding';
	my ($i, $ch, $l, $f);
	$l = length( $str ) - 1;
	for $i( 0 .. $l ) {
		$ch = ord( substr( $str, $i, 1 ) );
		if( $ch > 127 ) {
			$f = 1;
			last;
		}
	}
	$res .= ', ' . ($f ? 'non-' : '') . 'ASCII';
	$res .= ', ' . ($l + 1) . ' characters';
	$res .= ', ' . bytes::length( $str ) . ' bytes';
	return $res;
}

sub neat {
	my ($val, $maxlen) = @_;
	$maxlen or $maxlen = $DBI::neat_maxlen;
	if( ! defined $val ) {
		return 'undef';
	}
	my $l = length( $val ) - 1;
	my $v = utf8::valid( $val );
	$v and utf8::decode( $val );
	if( utf8::is_utf8( $val ) ) {
		$maxlen and $maxlen - 3 < $l and
			$val = substr( $val, 0, $maxlen - 3 ) . '...';
		utf8::encode( $val );
		return "\"$val\"";
	}
	my ($str, $i, $ch);
	$str = '';
	for $i( 0 .. $l ) {
		$ch = substr( $val, $i, 1 );
		$str .= ord( $ch ) > 127 ? '?' : $ch;
	}
	$maxlen and $maxlen - 3 < $l and
		$str = substr( $str, 0, $maxlen - 3 ) . '...';
	return "'$str'";
}

sub neat_list {
	my ($listref, $maxlen, $field_sep) = @_;
	join( $field_sep, map( &neat( $_, $maxlen ), @$listref ) );
}

sub looks_like_number {
	my @res;
	foreach( @_ ) {
		if( /^\s*0x\d+/ || /^\s*\d+\.\d+/ ) {
			push @res, 1;
		}
		else {
			push @res, 0;
		}
	}
	return @res;
}

sub hash {
	my ($str, $type) = @_;
	$type = int( $type );
	if( $type == 0 ) {
		use integer;
		my $l = length( $str ) - 1;
		my $r = 0;
		for my $i( 0 .. $l ) {
			$r = $r * 33 + ord( substr( $str, $i, 1 ) );
		}
		$r &= 0x7FFFFFFF;
		$r |= 0x40000000;
		no integer;
		return - $r;
	}
	elsif( $type == 1 ) {
		use integer;
		my $l = length( $str ) - 1;
		my $r = 0x811C9DC5;
		for my $i ( 0 .. $l ) {
			# DBI are you right? i thought first xor and then multiply?
			$r *= 0x1000193;
			$r ^= ord( substr( $str, $i, 1 ) );
		}
		$r &= 0xFFFFFFFF;
		no integer;
		# 32bit signed integer
		$r > 0x7FFFFFFF and $r = - (0x80000000 - ($r & 0x7FFFFFFF));
		return $r;
	}
	&Carp::croak( "DBI::hash($type): invalid type" );
}

################################ DBI::db ################################

package
	DBI::db;

BEGIN {
	$Carp::Internal{'DBI::db'} ++;
	*prepare_cached = \&prepare;
	*_warn = \&DBI::_warn;
	*set_err = \&DBI::set_err;
}

1;

sub disconnect {
	my $this = shift or return;
	$this->{'__con'}->free;
}

sub clone {
	my $this = shift or return;
	my $dsn = $this->{'__dsn'};
	my $con = DBE->connect( $dsn ) or return undef;
	my $new = { %$this };
	$new->{'__con'} = $con;
	$con->set_error_handler( \&DBI::_error_handler, $new );
	bless $new, 'DBI::db';
}

sub _prepare_statement {
	my ($this) = @_;
	my $dbh = $this->{'Database'};
	$this->{'PrintWarn'} = $dbh->{'PrintWarn'};
	$this->{'PrintError'} = $dbh->{'PrintError'};
	$this->{'RaiseError'} = $dbh->{'RaiseError'};
	$this->{'HandleError'} = $dbh->{'HandleError'};
	$this->{'FetchHashKeyName'} = $dbh->{'FetchHashKeyName'};
	$this->{'Type'} = 'st';
	$this->{'Executed'} = 0;
	my $stmt = $this->{'__stmt'};
	if( $stmt ) {
		$this->{'NUM_OF_PARAMS'} = 
			$this->{'__con'}->driver_has( 'param_count' )
				? $this->{'__stmt'}->param_count : 0;
	}
	$this->{'ParamValues'} = {};
	$this->{'ParamTypes'} = {};
}

sub prepare {
	my ($this, $statement, $attr) = @_;
	my $con = $this->{'__con'};
	if( $this->{'__autocommit'} != $this->{'AutoCommit'} ) {
		$con->auto_commit( $this->{'AutoCommit'} ) or return undef;
		$this->{'AutoCommit'} = $this->{'__autocommit'};
	}
	my $stmt = $con->prepare( $statement ) or return undef;
	$this = {
		'__con' => $con,
		'__stmt' => $stmt,
		'Database' => $this,
		'Statement' => $statement,
	};
	_prepare_statement( $this );
	bless $this, 'DBI::st';
}

sub do {
	my ($this, $statement, $attr) = (shift, shift, shift);
	my $con = $this->{'__con'};
	if( $this->{'__autocommit'} != $this->{'AutoCommit'} ) {
		$con->auto_commit( $this->{'AutoCommit'} ) or return undef;
		$this->{'AutoCommit'} = $this->{'__autocommit'};
	}
	$con->do( $statement, @_ ) or return undef;
	my $rows = $con->affected_rows;
	$rows ? $rows : '0E0';
}

sub selectrow_array {
	my ($this, $statement, $attr) = (shift, shift, shift);
	$this->{'__con'}->selectrow_array( $statement, @_ );
}

sub selectrow_arrayref {
	my ($this, $statement, $attr) = (shift, shift, shift);
	$this->{'__con'}->selectrow_arrayref( $statement, @_ );
}

sub selectrow_hashref {
	my ($this, $statement, $attr) = (shift, shift, shift);
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	$con->selectrow_hashref( $statement, @_ );
}

sub selectall_arrayref {
	my ($this, $statement, $attr) = (shift, shift, shift);
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	$con->selectall_arrayref( $statement, @_ );
}

sub selectall_hashref {
	my ($this, $statement, $key_field, $attr) = (shift, shift, shift, shift);
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	$con->selectall_hashref( $statement, $key_field, @_ );
}

sub selectcol_arrayref {
	my ($this, $statement, $attr) = (shift, shift, shift);
	if( $attr ) {
		if( my $cols = $attr->{'Columns'} ) {
			if( @$cols > 1 ) {
				my %res;
				$this->select_col( $statement,
					[\%res, $cols->[0], $cols->[1]], @_ );
				return \%res;
			}
			else {
				my @res;
				$this->select_col( $statement, [\@res, $cols->[0]], @_ );
				return \@res;
			}
		}
	}
	my @col = $this->select_col( $statement, @_ );
	return \@col;
}

sub last_insert_id {
	my ($this, $catalog, $schema, $table, $field, $attr) = @_;
	$this->{'__con'}->insert_id( $catalog, $schema, $table, $field );
}

sub begin_work {
	my $this = shift;
	$this->{'AutoCommit'} or
		return _warn( $this, 'begin_work ineffective without AutoCommit' );
	$this->{'__con'}->begin_work;
}

sub rollback {
	my $this = shift;
	$this->{'AutoCommit'} and
		return _warn( $this, 'rollback ineffective with AutoCommit' );
	$this->{'__con'}->rollback;
}

sub commit {
	my $this = shift;
	$this->{'AutoCommit'} and
		return _warn( $this, 'commit ineffective with AutoCommit' );
	$this->{'__con'}->commit;
}

sub ping {
	my $this = shift;
	$this->{'__con'}->ping();
}

sub get_info {
	my $this = shift;
	$this->{'__con'}->getinfo( @_ );
}

sub data_sources {
	my $this = shift;
	$this->{'__con'}->data_sources();
}

sub table_info {
	my ($this, $catalog, $schema, $table, $type, $attr) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->tables( $catalog, $schema, $table, $type )
		or return undef;
	$this = {
		'__con' => $con,
		'Database' => $this,
	};
	bless $this, 'DBI::st';
	&_prepare_statement( $this );
	&DBI::st::_prepare_result( $this, $res );
	return $this;
}

sub tables {
	my ($this, $catalog, $schema, $table, $type) = @_;
	my $res = $this->{'__con'}->tables( $catalog, $schema, $table, $type )
		or return undef;
	my (@row, @res);
	while( $res->fetch_row( \@row ) ) {
		push @res, $row[2];
	}
	return @res;
}

sub column_info {
	my ($this, $catalog, $schema, $table, $column) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->columns( $catalog, $schema, $table, $column )
		or return undef;
	$this = {
		'__con' => $con,
		'Database' => $this,
	};
	bless $this, 'DBI::st';
	&_prepare_statement( $this );
	&DBI::st::_prepare_result( $this, $res );
	return $this;
}

sub primary_key_info {
	my ($this, $catalog, $schema, $table) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->primary_keys( $catalog, $schema, $table )
		or return undef;
	$this = {
		'__con' => $con,
		'Database' => $this,
	};
	bless $this, 'DBI::st';
	&_prepare_statement( $this );
	&DBI::st::_prepare_result( $this, $res );
	return $this;
}

sub primary_key {
	my ($this, $catalog, $schema, $table) = @_;
	my $res = $this->{'__con'}->primary_keys( $catalog, $schema, $table )
		or return undef;
	my (@row, @res);
	while( $res->fetch_row( \@row ) ) {
		push @res, $row[3];
	}
	return @res;
}

sub foreign_key_info {
	my $this = shift;
	my ($pk_cat, $pk_schem, $pk_table, $fk_cat, $fk_schem, $fk_table) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->foreign_keys(
		$pk_cat, $pk_schem, $pk_table,
		$fk_cat, $fk_schem, $fk_table
	) or return undef;
	$this = {
		'__con' => $con,
		'Database' => $this,
	};
	bless $this, 'DBI::st';
	&_prepare_statement( $this );
	&DBI::st::_prepare_result( $this, $res );
	return $this;
}

sub statistics_info {
	my ($this, $catalog, $schema, $table, $unique_only, $quick) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->statistics( $catalog, $schema, $table, $unique_only )
		or return undef;
	$this = {
		'__con' => $con,
		'Database' => $this,
	};
	bless $this, 'DBI::st';
	&_prepare_statement( $this );
	&DBI::st::_prepare_result( $this, $res );
	return $this;
}

sub type_info_all {
	my ($this) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->type_info() or return undef;
	my $i = 0;
	my %names = map { $_ => $i ++ } $res->fetch_names();
	my @res = (\%names);
	while( my $row = $res->fetchrow_arrayref() ) {
		push @res, $row;
	}
	return \@res;
}

sub type_info {
	my ($this, $sql_type) = @_;
	my $con = $this->{'__con'};
	$con->name_convert( $this->{'FetchHashKeyName'} );
	my $res = $con->type_info( $sql_type ) or return undef;
	my @res;
	while( my $row = $res->fetchrow_hashref() ) {
		push @res, $row;
	}
	return @res;
}

sub quote {
	my $this = shift;
	$this->{'__con'}->quote( @_ );
}

sub quote_identifier {
	my $this = shift;
	$this->{'__con'}->quote_id( @_ );
}

sub trace {
	my( $this, $level, $fh ) = @_;
	$level or return $this->{'TraceLevel'};
	if( $fh && ! ref( $fh ) && -f $fh ) {
		open $fh, ">> $fh" or
			return &set_err( $this, -1, "cant open file: $!" );
	}
	$this->{'TraceLevel'} = $level;
	my @level = split( /[\|,]/, $level );
	$level[0] = int( $level[0] ) or return $this->{'__con'}->trace( 0 );
	if( uc( $level[1] ) eq 'SQL' ) {
		$level[0] = 2;
	}
	else {
		$level[0] = 3;
	}
	$this->{'__con'}->trace( $level[0], $fh );
}

sub errstr {
	my $this = shift;
	$this->{'__errstr'};
}

sub err {
	my $this = shift;
	$this->{'__err'};
}

sub can {
	my $this = shift;
	$this->{'__con'}->driver_has( @_ );
}

################################ DBI::st ################################

package
	DBI::st;

BEGIN {
	$Carp::Internal{'DBI::st'} ++;
	*_warn = \&DBI::_warn;
	*set_err = \&DBI::set_err;
	*can = \&DBI::db::can;
	*fetch = \&fetchrow_arrayref;
}

1;

sub bind {
	my $this = shift;
	my $stmt = $this->{'__stmt'} or return
		&set_err( $this, -1, "Bind is not supported by this statement" );
	$stmt->bind( @_ );
}

sub bind_param {
	my ($this, $p_num, $p_val, $p_type) = @_;
	my $stmt = $this->{'__stmt'} or return
		&set_err( $this, -1, "Bind param is not supported by this statement" );
	if( $p_type ) {
		my $t = int( $p_type );
		if( $t == 4 || $t == 5 || $t == -5 ) {
			$p_type = 'i';
		}
		elsif( $t == 3 || ($t >= 6 && $t <= 8) ) {
			$p_type = 'd';
		}
		else {
			$p_type = 'b';
		}
	}
	$stmt->bind_param( $p_num, $p_val, $p_type ) or return undef;
	$this->{'ParamValues'}->{$p_num} = $$p_val;
	$this->{'ParamTypes'}->{$p_num} = $p_type;
	return 1;
}

sub bind_param_inout {
	my ($this, $p_num, $p_var, $max_len, $p_type) = @_;
	my $stmt = $this->{'__stmt'} or return
		&set_err( $this, -1, "Bind param is not supported by this statement" );
	if( $p_type ) {
		my $t = int( $p_type );
		if( $t == 4 || $t == 5 || $t == -5 ) {
			$p_type = 'i';
		}
		elsif( $t == 3 || ($t >= 6 && $t <= 8) ) {
			$p_type = 'd';
		}
		else {
			$p_type = 'b';
		}
	}
	$stmt->bind_param_inout( $p_num, $p_var, $p_type ) or return undef;
	$this->{'ParamValues'}->{$p_num} = $$p_var;
	$this->{'ParamTypes'}->{$p_num} = $p_type;
	return 1;
}

sub bind_col {
	my ($this, $c_num, $c_val, $c_type) = @_;
	my $res = $this->{'__res'} or return
		&set_err( $this, -1, "Can't bind column without execute()" );
	$res->bind_column( $c_num, $c_val );
}

sub bind_columns {
	my $this = shift;
	my $res = $this->{'__res'} or return
		&set_err( $this, -1, "Can't bind columns without execute()" );
	$res->bind( @_ );
}

sub _prepare_result {
	my( $this, $res ) = @_;
	my $rows;
	$this->{'Executed'} = 1;
	$this->{'FetchHashKeyName'} ||= 'NAME';
	$this->{'PRECISION'} = [];
	$this->{'SCALE'} = [];
	$this->{'NULLABLE'} = [];
	if( ref( $res ) ) {
		$this->{'__con'}->name_convert( $this->{'FetchHashKeyName'} );
		$this->{'__res'} = $res;
		$rows = $res->num_rows;
		$this->{'NUM_OF_FIELDS'} = $res->num_fields;
		my @name = $res->fetch_names( 'org' );
		my @name_lc = $res->fetch_names( 'lc' );
		my @name_uc = $res->fetch_names( 'uc' );
		$this->{'NAME'} = \@name;
		$this->{'NAME_lc'} = \@name_lc;
		$this->{'NAME_uc'} = \@name_uc;
	}
	else {
		$rows = $this->{'__con'}->affected_rows;
		$this->{'NUM_OF_FIELDS'} = 0;
		$this->{'NAME'} = [];
		$this->{'NAME_lc'} = [];
		$this->{'NAME_uc'} = [];
	}
	$DBI::rows = $rows;
	$rows ? $rows : '0E0';
}

sub execute {
	my $this = shift;
	my $stmt = $this->{'__stmt'} or return;
	my $res = $stmt->execute( @_ ) or return undef;
	&_prepare_result( $this, $res );
}

sub fetchrow_array {
	my $this = shift;
	$this->{'__res'} or return
		&_warn( $this, "fetchrow_array: fetch() without execute()" );
	$this->{'__res'}->fetchrow_array;
}

sub fetchrow_arrayref {
	my $this = shift;
	$this->{'__res'} or return
		&_warn( $this, "fetchrow_arrayref: fetch() without execute()" );
	$this->{'__res'}->fetchrow_arrayref;
}

sub fetchrow_hashref {
	my $this = shift;
	$this->{'__res'} or return
		&_warn( $this, "fetchrow_hashref: fetch() without execute()" );
	$this->{'__res'}->fetchrow_hashref;
}

sub fetchall_arrayref {
	my $this = shift;
	$this->{'__res'} or return
		&_warn( $this, "fetchall_arrayref: fetch() without execute()" );
	$this->{'__res'}->fetchall_arrayref( @_ );
}

sub fetchall_hashref {
	my $this = shift;
	$this->{'__res'} or return
		&_warn( $this, "fetchall_hashref: fetch() without execute()" );
	$this->{'__res'}->fetchall_hashref( @_ );
}

sub dump_results {
	my ($this, $maxlen, $lsep, $fsep, $fh) = @_;
	$fh ||= \*STDOUT;
	$fsep ||= ', ';
	$lsep ||= "\n";
	$maxlen ||= 35;
	my $res = $this->{'__res'} or return
		&_warn( $this, "dump_results: fetch() without execute()" );
	my $i = 0;
	while( my $row = $res->fetchrow_arrayref ) {
		print $fh &DBI::neat_list( $row, $maxlen, $fsep ), $lsep;
		$i ++;
	}
	print $fh "$i rows$lsep";
}

sub finish {
	my $this = shift;
	my $res = $this->{'__res'} or return;
	$res->free;
	delete $this->{'__res'};
}

sub rows {
	my $this = shift;
	$this->{'__res'} or return 0;
	$this->{'__res'}->num_rows;
}

sub errstr {
	my $this = shift;
	$this->{'Database'}->error;
}

sub err {
	my $this = shift;
	$this->{'Database'}->errno;
}

__END__

=head1 NAME

DBE::CompatDBI - Emulate DBI with DBE


=head1 SYNOPSIS

  use DBE::CompatDBI qw(neat_list);
  
  $dbh = DBI->connect( "dbi:DriverName:Options" );
  $sth = $dbh->prepare( "SELECT * FROM person" );
  $sth->execute();
  print join( ', ', @{$sth->{'NAME'}} ), "\n";
  while( $row = $sth->fetchrow_arrayref() ) {
      print &neat_list( $row ), "\n";
  }

=head1 DESCRIPTION

DBE::CompatDBI is designed for switching to DBE without rewriting
existing DBI scripts.
You just need to replace the "use DBI" statement with "use DBE::CompatDBI".
Since it uses the DBI namespace it is not possible to run the DBI module and
DBE::CompatDBI at the same time.

=head1 LIMITATIONS

Driver handles are not supported.

Common methods I<trace>, I<trace_msg>, I<func>, I<parse_trace_flags>,
I<parse_trace_flag>, I<private_attribute_info> and I<swap_inner_handle> are
not supported.

Database method I<take_imp_data> is not supported.

Statement methods I<bind_param_inout>, I<bind_param_array> and
I<execute_array> are not supported.

Statement attributes I<TYPE>, I<PRECISION>, I<SCALE>, I<NULLABLE>,
I<CursorName> and I<RowsInCache> are not supported.

DBI environment variables are not supported.

=head1 AUTHORS

Written by Christian Mueller

=head1 COPYRIGHT

The DBE::CompatDBI module is free software. You may distribute under the terms
of either the GNU General Public License or the Artistic License, as specified
in the Perl README file.

=cut
