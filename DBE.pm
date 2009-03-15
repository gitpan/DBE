package DBE;
# =============================================================================
# DBE - Database (Express) Engine for Perl
# =============================================================================

# uncomment for debugging
#use strict;
#use warnings;

our ($VERSION, $MODPERL);

BEGIN {
	$VERSION = '0.99_1';
	
	require XSLoader;
	XSLoader::load( __PACKAGE__, $VERSION );
	
	# setup links
	*DBE::CON::do = \&DBE::CON::query;
	*DBE::CON::disconnect = \&DBE::CON::close;
	*DBE::RES::fetchrow_array = \&DBE::RES::fetch_row;
	*DBE::RES::names = \&DBE::RES::fetch_names;
    
    # mod_perl
	if( exists $ENV{'MOD_PERL_API_VERSION'} &&
		$ENV{'MOD_PERL_API_VERSION'} == 2
	) {
		$MODPERL = 2;
		require Apache2::ServerUtil;
	}
	elsif( defined $Apache::VERSION &&
		$Apache::VERSION > 1 && $Apache::VERSION < 1.99
	) {
		$MODPERL = 1;
	}
	else {
		$MODPERL = 0;
    }
}

1; # return

sub __reg_cleanup__ {
	if( $PerlCGI::VERSION ) {
		App::register_cleanup( \&__cleanup__ );
	}
	elsif( $MODPERL == 2 ) {
		# needs global request enabled
    	Apache2::RequestUtil->request->pool->cleanup_register( \&__cleanup__ );
    }
	elsif( $MODPERL == 1 ) {
    	Apache->request->register_cleanup( \&__cleanup__ );
    }
}

sub __caller__ {
	# how to replace it with XS code?
	return reverse CORE::caller( $_[0] );
}

#sub import {
#	$DBE::Const::VERSION or require DBE::Const;
#	$DBE::Const::ExportLevel = 1;
#	eval {
#		&DBE::Const::import( @_ );
#	};
#	if( $@ ) {
#		my $err = $@;
#		$err =~ s/\s+at\s+.+?\s+line\s+\d+\.\s*//;
#		$Carp::VERSION or require Carp;
#		&Carp::croak( $err );
#	}
#	$DBE::Const::ExportLevel = 0;
#}

__END__

=head1 NAME

DBE - Database (Express) Engine for Perl


=head1 SYNOPSIS

  use DBE;
  
  $con = DBE->connect( %arg );
  $con = DBE->connect( $dsn );
  
  $res = $con->query( $sql );
  $res = $con->query( $sql, @bind_values );
  
  $rv = $con->do( $sql );
  $rv = $con->do( $sql, @bind_values );
  
  $con->prepare( $sql );
  $res = $con->execute( ... );
  
  $stmt = $con->prepare( $sql );
  $stmt->bind_param( $p_num, $value );
  $stmt->bind_param( $p_num, $value, $type );
  
  $res = $stmt->execute();
  $res = $stmt->execute( @bind_values );
  
  @row = $res->fetch_row();
  $res->fetch_row( \@row );
  $res->fetch_row( \%row );
  
  @row = $res->fetchrow_array();
  $row = $res->fetchrow_arrayref();
  $row = $res->fetchrow_hashref();
  
  $array = $res->fetchall_arrayref();
  $hash = $res->fetchall_hashref( $key );
  
  @name = $res->fetch_names();
  $num_rows = $res->num_rows();
  $num_fields = $res->num_fields();
  
  $rv = $con->auto_commit( $mode );
  $rv = $con->begin_work();
  $rv = $con->commit();
  $rv = $con->rollback();
  
  $str   = $con->error();
  $errno = $con->errno();
  
  $rv = $con->set_charset( $charset );
  $charset = $con->get_charset();
  
  $quoted = $db->quote( $arg );
  $quoted = $db->quote_id( ... );


=head1 DESCRIPTION

DBE provides an interface for high performance database communication.

The goal for this module is a fast and efficient database interface with
the power of DBI.

The documentation uses different variable names for different classes.
These variables are defined as follows.
I<$con> defines a connection class, I<$res> defines a result set class,
I<$stmt> defines a statement class and I<$lob> defines a class of a large
object. 

=head2 Uniform SQL

Functions with SQL statements offer an extended notation of catalog, schema
and table names.
If a SQL statement starts with a percent (%) sign identities can be
written uniformly.
The identities stay in square brackets []. Within the notation points (.) are
evaluated as separators for schema, table and field names.
The catalog is identified by an AT (@) character. It must be the last part
in the identity.
The route (#) character is converted into a point.

B<Examples>

MySQL Driver

  # uniform
  $con->query("% SELECT * FROM [table@db] WHERE [field] = '1'");
  # native
  $con->query("SELECT * FROM `db`.`table` WHERE `field` = '1'");

PostgreSQL Driver

  # uniform
  $con->query("% SELECT * FROM [schema.table] AS [t1] WHERE [t1.id] = 1");
  # native
  $con->query('SELECT * FROM "schema"."table" AS "t1" WHERE "t1"."id" = 1');

Text Driver

  # uniform
  $con->query('% SELECT * FROM [table#csv] WHERE [table.field] = 1');
  # native
  $con->query('SELECT * FROM "table.csv" WHERE "table"."field" = 1');

=head3 Parameter Names in Placeholders for All

Uniform SQL enables you to use parameter names in placeholders.
A parameter name begins with a colon (:) character and may contain one or
more of the following characters: 'a'-'z', 'A'-'Z', '0'-'9', '_'. 
The names are converted into question mark (?) characters before the
statement is sent to the driver.

B<Examples>

  # prepare, execute
  $stmt = $con->prepare( '% SELECT * FROM [table] WHERE [id] > :id' );
  $stmt->bind_param( ':id', 1000, 'i' );
  $res = $stmt->execute();
  
  # or as query
  $res = $con->query(
      '% SELECT * FROM [table] WHERE [id] > :id',
      { 'id' => 1000 }
  );

=head2 Functions by Category

B<Main Functions>

=over

=item

C<affected_rows>,
C<bind_param>,
C<connect>,
C<disconnect>,
C<do>,
C<execute>,
C<get_charset>,
C<insert_id>,
C<param_count>,
C<prepare>,
C<query>,
C<set_charset>

=back

B<Accessing Result Data>

=over

=item

C<bind_column>,
C<dump>,
C<fetch>,
C<fetch_col>,
C<fetch_row>,
C<fetchall_arrayref>,
C<fetchall_hashref>,
C<fetchrow_arrayref>,
C<fetchrow_hashref>,
C<num_rows>,
C<row_seek>,
C<row_tell>

=back

B<Accessing Fields in a Result Set>

=over

=item

C<fetch_field>,
C<fetch_names>,
C<field_seek>,
C<field_tell>,
C<names>,
C<num_fields>

=back

B<Transactions>

=over

=item

C<auto_commit>,
C<begin_work>,
C<commit>,
C<rollback>

=back

B<Combined Selects>

=over

=item

C<selectall_arrayref>,
C<selectall_hashref>,
C<selectrow_array>,
C<selectrow_arrayref>,
C<selectrow_hashref>

=back

B<Information and Catalog Functions>

=over

=item

C<columns>,
C<data_sources>,
C<foreign_keys>,
C<getinfo>,
C<primary_keys>,
C<installed_drivers>,
C<open_drivers>,
C<special_columns>,
C<tables>,
C<type_info>

=back

B<Misc Functions>

=over

=item

C<attr_get>,
C<attr_set>,
C<driver_has>,
C<escape_pattern>,
C<name_convert>,
C<row_limit>,
C<quote>,
C<quote_id>,
C<ping>

=back

B<Error Handling>

=over

=item

C<errno>,
C<error>,
C<set_error_handler>,
C<trace>

=back

B<Large Objects>

=over

=item

C<close>,
C<eof>,
C<getc>,
C<read>,
C<print>,
C<seek>,
C<size>,
C<tell>,
C<write>

=back

=head2 Examples

=head3 Simple Query

  use DBE;
  
  $con = DBE->connect(
      'provider' => 'MySQL',
      'socket' => '/tmp/mysql.sock',
      'user' => 'root',
      'auth' => '',
      'db' => 'test',
      'charset' => 'utf8',
  );
  
  $res = $con->query( "SELECT * FROM table WHERE field = 'foo'" );
  
  print join( '|', $res->fetch_names() ), "\n";
  while( @row = $res->fetch_row() ) {
      print join( '|', @row ), "\n";
  }


=head3 Query with statements

  use DBE;
  
  $dsn = "Provider=MySQL;Host=localhost;User=root;Password=;Database=test";
  $con = DBE->connect( $dsn );
  $con->set_charset( 'utf8' );
  
  $stmt = $con->prepare( 'SELECT * FROM table WHERE field = ?' );
  # bind "foo" to parameter 1 and execute
  $res = $stmt->execute( 'foo' );
  
  print join( '|', $res->fetch_names() ), "\n";
  while( $row = $res->fetchrow_arrayref() ) {
      print join( '|', @$row ), "\n";
  }


=head1 METHODS

=head2 Connection Control Methods

=over 2

=item B<$con = DBE -E<gt> connect ( %hash )>

=item B<$con = DBE -E<gt> connect ( $dsn )>

Opens a connection to a database.

B<Parameters>

=over

=item I<%hash>

Following parameters are support by DBE:

=over

=item * B<PROVIDER> | B<DRIVER> [string]

Name of provider to connect with. Defaults to "Text".

=item * B<WARN> [1|0|Yes|No|True|False]

Warn on error. Defaults to $^W.

=item * B<CROAK> [1|0|Yes|No|True|False]

Croak on error. Defaults to TRUE.

=item * B<RECONNECT> [int]

Number of attempts to reconnect with a delay of 1 sec. Default value "0"
disables reconnection.

=back

=item I<$dsn>

The connection string that includes the provider name, and other parameters
needed to establish the initial connection.

The basic format of a connection string includes a series of keyword/value
pairs separated by semicolons. The equal sign (=) connects each keyword
and its value. To include values that contain a semicolon, single-quote
character, or double-quote character, the value must be enclosed in double
quotation marks. If the value contains both a semicolon and a double-quote
character, the value can be enclosed in single quotation marks. The single
quotation mark is also useful if the value starts with a double-quote
character. Conversely, the double quotation mark can be used if the value
starts with a single quotation mark. If the value contains both single-quote
and double-quote characters, the quotation-mark character used to enclose
the value must be doubled every time it occurs within the value.
Parameters are listed below.

=back

All parameters are case-insensitive.
Additional parameters are documented in the drivers manual.

B<Return Values>

Returns a connection object (I<$con>) on success, or undef on failure. 

B<Examples>

  # parameters as hash
  $con = DBE->connect(
      'provider' => 'Text',
      'dbq' => '/path/to/csv-files/',
  );
  
  # parameters as dsn
  $dsn = 'Provider=Text;DBQ=.;Format="Delimited(;)";QuoteBy=""""';
  $con = DBE->connect( $dsn );


=item B<$con -E<gt> close ()>

=item B<$con -E<gt> disconnect ()>

Closes the connection explicitly and frees its resources.
I<disconnect()> is a synonym for I<close()>.


=item B<$con -E<gt> set_charset ( $charset )>

Sets the default character set to be used when sending data from and to the
database server.

B<Parameters>

=over

=item I<$charset>

The character set to be set.

=back

B<Return Values>

Returns a TRUE value on success or undef on failure.

=item B<$con -E<gt> get_charset ()>

Gets the default character set.

B<Return Values>

Returns the default character set or undef on error.


=item B<DBE -E<gt> errno ()>

=item B<$con -E<gt> errno ()>

Returns the last error code for the most recent function call that can
succeed or fail.

B<Return Values>

An error code value for the last call, if it failed. Zero means no error
occurred.

B<Examples>

  # global error
  $con = DBE->connect( ... )
      or die 'Connect failed (' . DBE->errno . ') ' . DBE->error;
  
  # connection error
  $con->query( ... )
      or die 'Query failed (' . $con->errno . ') ' . $con->error;
  

=item B<DBE -E<gt> error ()>

=item B<$con -E<gt> error ()>

Returns the last error message for the most recent function call that can
succeed or fail.

B<Return Values>

A string that describes the error. An empty string if no error occurred.

B<Examples>

  # global error
  $con = DBE->connect( ... )
      or die 'Connect failed ' . DBE->error;
  
  # connection error
  $con->query( ... )
      or die 'Query failed ' . $con->error;
  

=item B<$con -E<gt> set_error_handler ( [$fnc [, $arg]] )>

Sets or removes a userdefined error handler.

B<Parameters>

=over

=item I<$fnc>

Function name or code reference.

=item I<$arg>

Argument can be used to pass a class reference.

=back

B<Return Values>

Resturns true on success, or undef on error.

B<Parameters passed to error function>

=over

=item I<$code>

The error code.

=item I<$msg>

The error message.

=item I<$provider>

The description string of the provider.

=item I<$action>

Performed action during the error occurred.

=item I<$obj_id>

Either a value of "con", "res" or "stmt".

=item I<$obj>

Depending on I<$obj_id>, it can be a connection object, a result object or
a statement object.

=back

B<Examples>

Error handler

  $con = DBE->connect ('Provider=Text');
  
  # set error handler to a function
  $con->set_error_handler (\&error_handler);
  
  # raise a syntax error
  $con->do ("SELECT * FROM");
  
  sub error_handler {
      # does allmost the same like internal croak
      my ($code, $msg, $provider, $action, $obj_id, $obj) = @_;
      my ($pkg, $file, $line) = caller ();
      print STDERR "[$provider] $action: $msg at $file line $line\n";
      exit;
  }

Error handler as class function

  $con = DBE->connect ('Provider=Text');
  
  $obj = MY->new ();
  
  # set error handler to an object
  $con->set_error_handler ('error_handler', $obj);
  # or
  # $con->set_error_handler ('MY::error_handler', $obj);
  # or
  # $con->set_error_handler (\&MY::error_handler, $obj);
  
  # raise a syntax error
  $con->do ("SELECT * FROM");
  
  1;
  
  package MY;
  
  require Carp;
  
  sub new {
      bless {}, shift;
  }
  
  sub error_handler {
      # does allmost the same like internal croak
      my ($this, $code, $msg, $provider, $action, $obj_id, $obj) = @_;
      &Carp::croak ("[$provider] $action: $msg");
  }


=item B<DBE -E<gt> trace ( [$level [, $iohandle]] )>

=item B<$con -E<gt> trace ( [$level [, $iohandle]] )>

Enables tracing support with a specified level.
The levels are defined as follows:

=for formatter none

  0 - DBE_TRACE_NONE - disable tracing
  1 - DBE_TRACE_SQL - trace sql statements only
  2 - DBE_TRACE_SQLFULL - trace sql statements inlcuding bind
      and execute calls
  3 - DBE_TRACE_ALL - trace all messages

=for formatter perl

Setting trace options to a connection object will overwrite global trace
settings.

The I<$iohandle> parameter can contain an io handle to write the messages
there. The default handle is STDERR.

B<Examples>

  # global tracing of all messages
  DBE->trace( 3 );
  
  # trace into a file
  open( $fh, "> trace.log" ) or die "can't open file: $!";
  DBE->trace( 3, $fh );
  
  # just trace the sql statements within a connection object
  $con->trace( 1 );
  
  # disable global tracing
  DBE->trace( 0 );

=back

=head2 Command Execution Methods

=over 2

=item $res = B<$con -E<gt> query ( $statement )>

=item $res = B<$con -E<gt> query ( $statement, @bind_values )>

=item $res = B<$con -E<gt> do ( $statement )>

=item $res = B<$con -E<gt> do ( $statement, @bind_values )>

Sends a SQL statement to the currently active database on the server. 
I<do()> is a synonym for I<query()>.

B<Parameters>

=over

=item I<$statement>

This parameter can include one or more parameter markers in the SQL statement
by embedding question mark (?) characters at the appropriate positions. 

=item I<@bind_values>

An array of values to bind to the statement.
Values bound in this way are usually treated as "string" types unless
the driver can determine the correct type, or unless I<bind_param()> has
already been used to specify the type.

=back

B<Return Values>

For selective queries I<query()> returns a result class (I<$res>) on success,
or FALSE on error. 

For other type of SQL statements, UPDATE, DELETE, DROP, etc,
I<query()> returns TRUE on success or FALSE on error. 


=item $stmt = B<$con -E<gt> prepare ( $statement )>

Prepares the SQL query pointed to by the null-terminated string query, and
returns a statement handle to be used for further operations on the statement.
The query must consist of a single SQL statement. 

B<Parameters>

=over

=item I<$statement>

The query, as a string. 

This parameter can include one or more parameter markers in the SQL statement
by embedding question mark (?) characters at the appropriate positions. 

=back

B<Return Values>

Returns a statement class (I<$stmt>) or FALSE on failure.

B<Examples>

I<With statement>

  $stmt = $con->prepare( 'UPDATE table WHERE field = ?' );
  # bind 'foo' to 'field' as type 'string'
  $stmt->bind_param( 1, 'foo', 's' );
  # execute
  $res = $stmt->execute();

I<Without statement>

  $con->prepare( 'UPDATE table WHERE field = ?' );
  # bind 'foo' to 'field' as type 'string'
  $con->bind_param( 1, 'foo', 's' );
  # execute
  $res = $con->execute();

B<See Also>

C<execute()>, C<bind_param()>


=item B<$stmt -E<gt> param_count ()>

Returns the number of parameters in the statement.


=item B<$stmt -E<gt> bind_param ( $p_num )>

=item B<$stmt -E<gt> bind_param ( $p_num, $value )>

=item B<$stmt -E<gt> bind_param ( $p_num, $value, $type )>

Binds a value to a prepared statement as parameter

B<Parameters>

=over

=item I<$p_num>

The number of parameter starting at 1.

=item I<$value>

Any scalar value.

=item I<$type>

A string that contains one character which specify the type for the
corresponding bind value: 

=for formatter none

  Character Description
  ---------------------
  i   corresponding value has type integer 
  d   corresponding value has type double 
  s   corresponding value has type string 
  b   corresponding value has type binary 

=for formatter perl

=back

B<Return Values>

Returns a true value on success, or undef on error.

B<Example>

  $stmt = $con->prepare(
      'UPDATE table WHERE field1 = ? AND field2 = ?' );
  # bind '100' to 'field1' as type 'integer'
  $stmt->bind_param( 1, 100, 'i' );
  # bind 'foo' to 'field2'
  $stmt->bind_param( 2, 'foo' );

B<See Also>

C<prepare()>, C<execute()>


=item B<$stmt -E<gt> bind ( @bind_values )>

Values bound in this way are usually treated as "string" types unless
the driver can determine the correct type.

B<Return Values>

Returns a true value on success, or undef on error.


=item $res = B<$stmt -E<gt> execute ()>

=item $res = B<$stmt -E<gt> execute ( @bind_values )>

Executes a prepared statement.

B<Parameters>

=over

=item I<@bind_values>

An array of values to bind. Values bound in this way are usually treated as
"string" types unless the driver can determine the correct type.

=back

B<Return Values>

For SELECT, SHOW, DESCRIBE, EXPLAIN and other statements returning resultset,
query returns a result class (I<$res>) on success, or undef on error.

For other type of SQL statements, UPDATE, DELETE, DROP, etc, query returns
a true value on success, or undef on error.

B<Example>

  $stmt = $con->prepare( 'UPDATE table WHERE field = ?' );
  # bind 'foo' to 'field' and execute
  $res = $stmt->execute( 'foo' );

B<Note>

Some drivers (like MySQL) do not support different result sets within the
same statement. In this case each result set of the statement share the
same data.

B<See Also>

C<prepare()>, C<bind_param()>


=item B<$stmt -E<gt> close ()>

Closes a statement explicitly.


=back

=head3 Retrieving Query Result Information

=over 2

=item B<$con -E<gt> affected_rows ()>

Gets the number of affected rows in a previous SQL operation
After executing a statement with C<query()> or
C<execute()>, returns the number
of rows changed (for UPDATE), deleted (for DELETE), or inserted (for INSERT).
For SELECT statements, I<affected_rows()> works like
C<num_rows()>. 

B<Return Values>

An integer greater than zero indicates the number of rows affected or retrieved.
Zero indicates that no records where updated for an UPDATE statement,
no rows matched the WHERE clause in the query or that no query has yet
been executed.


=item B<$con -E<gt> insert_id ()>

=item B<$con -E<gt> insert_id ( $catalog )>

=item B<$con -E<gt> insert_id ( $catalog, $schema )>

=item B<$con -E<gt> insert_id ( $catalog, $schema, $table )>

=item B<$con -E<gt> insert_id ( $catalog, $schema, $table, $column )>

Returns the auto generated id used in the last query or statement. 

B<Parameters>

=over

=item I<$catalog>

The catalog where the table is located.

=item I<$schema>

The schema where the table is located.

=item I<$table>

The table where the column is located.

=item I<$column>

The column to retrieve the generated id from.

=back

B<Return Values>

The value of an AUTO_INCREMENT (IDENDITY,SERIAL) field that was updated by the
previous query.
Returns NULL if there was no previous query on the connection or if the query
did not update an AUTO_INCREMENT value. 


=back

=head4 Accessing Result Data

=over 2

=item B<$res -E<gt> dump ( [$maxlen [, $lsep [, $fsep [, $fh [, $header]]]]] )>

Fetches all the rows from the result set and prints the results to a handle.
NULL fields are printed as undef. Fields with utf8 data are enclosed in double
quotes. Other fields are enclosed in single quotes.

B<Parameters>

=over

=item I<$maxlen>

The maximum length of a field value. Defaults to 35

=item I<$lsep>

Line separator, printed after each row. Defaults to "\n"

=item I<$fsep>

Field separator. Defaults to ", "

=item I<$fh>

File handle to print the results there. Defaults to STDOUT

=item I<$header>

Print the column names on the first line. Defaults to TRUE.

=back

B<Note>

This method is designed for testing purposes and should not be used to export
data.


=item B<$res -E<gt> fetch ()>

Fetches the next row into bound variables. See more at C<bind_column()>.


=item B<$res -E<gt> fetch_row ()>

=item B<$res -E<gt> fetch_row ( \@row )>

=item B<$res -E<gt> fetch_row ( \%row )>

=item B<$res -E<gt> fetchrow_array ()>

Get a result row as an array or a hash.
I<fetchrow_array()> is a synonym for I<fetch_row()>.

B<Paramters>

=over

=item I<\@row>

A reference to an array which will be filled with the data of the next row.

=item I<\%row>

A reference to a hash will be filled with the data of the next row, where
each key in the hash represents the name of one of the result set columns.

=back

B<Return Values>

Without parameters it returns an array of values that is filled with the
data of the next row. With parameters it returns TRUE.
FALSE is returned if there are no more rows in result set.

B<Examples>

  $res = $con->query( "SELECT prename, name FROM person" );
  
  $res->fetch_row( \%row );
  print "First person is $row{'prename'} $row{'name'}\n";
  
  $res->fetch_row( \@row );
  print "Second person is $row[0] $row[1]\n";
  
  @row = $res->fetch_row;
  print "Third person is $row[0] $row[1]\n";


=item B<$res -E<gt> fetch_col ()>

=item B<$res -E<gt> fetch_col ( $num )>

=item B<$res -E<gt> fetch_col ( \@col )>

=item B<$res -E<gt> fetch_col ( \@col, $num )>

=item B<$res -E<gt> fetch_col ( \%col )>

=item B<$res -E<gt> fetch_col ( \%col, $num_key, $num_val )>

Fetch all rows from the result set and return a column of it.

B<Paramters>

=over

=item I<\@col>

A reference to an array to store the data into.

=item I<$num>

A column number between 0 and C<num_fields()> - 1. Defaults to 0.

=item I<\%col>

A reference to a hash to store the data into. Each key in the hash
contains the data of column I<$num_key> and each value contains the data
of column I<$num_val>.

=item I<$num_key>

Column number to fetch as key. Defaults to 0.

=item I<$num_val>

Column number to fetch as value. Defaults to 1.

=back

B<Return Values>

Without I<\@col> and I<\%col> paramters it returns an array of values that
corresponds to the specified column of each row in the result set,
or TRUE on success, or FALSE if no data is available.

B<Examples>

I<Fetch columns as array>

  $res = $con->query( "SELECT name FROM color" );
  
  @colors = $res->fetch_col();
      or
  $res->fetch_col( \@colors );
  
  print "Available colors: ", join( ', ', @colors ), "\n";

I<Fetch columns as hash>

  $res = $con->query( "SELECT id, name FROM color" );
  $res->fetch_col( \%color );
  
  $res = $con->query( "SELECT color_id, description FROM articles" );
  $res->bind( $color_id, $desc );
  while( $res->fetch ) {
      print "Article $desc has color ", $color{$color_id}, "\n";
  }


=item B<$res -E<gt> fetchrow_arrayref ()>

Fetch a result row as an enumerated array.

B<Return Values>

Returns a reference to an array of values that corresponds to the fetched
row, or FALSE if there are no more rows in result set.


=item B<$res -E<gt> fetchrow_hashref ()>

=item B<$res -E<gt> fetchrow_hashref ( $name )>

Fetch a result row as an associative array (hash).

B<Paramters>

=over

=item I<$name>

Name conversation. A value of "lc" convert names to lowercase
and a value of "uc" convert names to uppercase.

=back

B<Return Values>

Returns a reference to a hash of values representing the
fetched row in the result set, where each key in the hash represents the
name of one of the result set columns or FALSE if there are no more rows
in the resultset. 

If two or more columns of the result have the same field names, the last
column will take precedence. To access the other columns of the same name,
you either need to access the result with numeric indices by using
C<fetch_row()>, C<fetchrow_arrayref()> or add alias names.


=item B<$res -E<gt> fetchall_arrayref ()>

=item B<$res -E<gt> fetchall_arrayref ( {} )>

Fetch all data from the result as a reference to an array, which contains a
reference to each row.

B<Paramters>

=over

=item I<{}>

Fetch all fields of every row as a hash ref.

=back

B<Return Values>

Returns a reference to an array of references to all fetched rows,
or FALSE if no data is available.

B<Examples>

  $res = $con->query( "SELECT name, age FROM person" );
  
  $a_per = $res->fetchall_arrayref();
  foreach $row( @$a_per ) {
      print $row->[0], " is ", $row->[1], " years old\n";
  }
  
  # fetch as hashref
  $a_per = $res->fetchall_arrayref( {} );
  foreach $row( @$a_per ) {
      print $row->{'name'}, " is ", $row->{'age'}, " years old\n";
  }


=item B<$res -E<gt> fetchall_hashref ( $key1 )>

=item B<$res -E<gt> fetchall_hashref ( $key1, $key2, ... )>

=item B<$res -E<gt> fetchall_hashref ( [$key1, $key2, ...] )>

Fetch all data from the result as a reference to hash containing a key for
each distinct value of the $key(n) column that was fetched.

B<Parameters>

=over

=item I<$key(n)>

Column name to use their values as key in the hash.

=back

B<Return Values>

Returns a reference to a hash containing a key for each distinct value
of the $key(n) column that was fetched. For each key the corresponding
value is a reference to a hash containing all the selected columns and
their values.

B<Examples>

  $res = $con->query( 'SELECT ID, Name FROM Table' );
  $data = $res->fetchall_hashref( 'ID' );
  # print name of ID = 2
  print $data->{2}->{'Name'};
  
  $res = $con->query( 'SELECT ID1, ID2, Name FROM Table' );
  $data = $res->fetchall_hashref( 'ID1', 'ID2' );
  # print name of ID1 = 2 and ID2 = 10
  print $data->{2}->{10}->{'Name'};


=item B<$res -E<gt> bind_column ( $c_num, $c_var )>

Binds a variable to a column in the result set.

B<Parameters>

=over

=item I<$c_num>

Number of column starting at 1.

=item I<$c_var>

A variable to bind. If it is a reference to a variable it will be dereferenced
once.

=back

B<Return Values>

Returns a true value on succes, or undef on error.

B<Examples>

  my ($article_id, $article_name);
  $res = $con->query( "SELECT ID, Name FROM Article" );
  $res->bind_column( 1, $article_id );
  $res->bind_column( 2, $article_name );
  
  while( $res->fetch ) {
      print "article $article_id: $article_name\n";
  }


=item B<$res -E<gt> bind ( @bind_variables )>

Binds one or more variables to the columns in the result set. It is a shorthand
version of C<bind_column()>.

B<Examples>

  my ($prename, $name, $age);
  $res = $con->query( "SELECT Prename, Name, Age FROM Person" );
  $res->bind( $prename, $name, $age );
  while( $res->fetch ) {
      print "$name, $prename is $age years old\n";
  }

I<Bind a hash>

  my %row;
  $res = $con->query( "SELECT Prename, Name, Age FROM Person" );
  $res->bind( @row{$res->names('lc')} );
  while( $res->fetch ) {
      print "$row{'name'}, $row{'prename'} is $row{'age'} years old\n";
  }


=item B<$res -E<gt> num_rows ()>

Get the number of rows in a result.

B<Return Values>

Returns number of rows in the result set.


=item B<$res -E<gt> row_tell ()>

Get the actual position of row cursor in a result (Starting at 0).

B<Return Values>

Returns the actual position of row cursor in a result.


=item B<$res -E<gt> row_seek ( $offset )>

Set the actual position of row cursor in a result (Starting at 0).

B<Paramters>

=over

=item I<$offset>

Absolute row position. Valid between 0 and C<num_rows()> - 1.

=back

B<Return Values>

Returns the previous position of row cursor in a result.


=back

=head4 Accessing Fields in a Result Set

=over 2

=item B<$res -E<gt> names ()>

=item B<$res -E<gt> names ( $name )>

=item B<$res -E<gt> fetch_names ()>

=item B<$res -E<gt> fetch_names ( $name )>

Returns an array of field names representing in a result set. I<names> is a
synonym for I<fetch_names>.

B<Paramters>

=over

=item I<$name>

Name conversation. A value of "lc" convert names to lowercase
and a value of "uc" convert names to uppercase.

=back

B<Return Values>

Returns an array of field names or FALSE if no field information is available. 


=item B<$res -E<gt> num_fields ()>

Gets the number of fields (columns) in a result.

B<Return Values>

Returns number of fields in the result set.


=item B<$res -E<gt> fetch_field ()>

Returns the next field in the result set.

B<Return Values>

Returns a hash which contains field definition information or FALSE if no
field information is available. 


=item B<$res -E<gt> field_tell ()>

Gets the actual position of field cursor in a result (Starting at 0).

B<Return Values>

Returns the actual position of field cursor in the result.


=item B<$res -E<gt> field_seek ( $offset )>

Sets the actual position of field cursor in the result (Starting at 0).

B<Paramters>

=over

=item I<$offset>

Absolute field position. Valid between 0 and
C<num_fields()> - 1.

=back

B<Return Values>

Returns the previous position of field cursor in the result.

=back

=head2 Transaction Methods

=over 2

=item B<$con -E<gt> auto_commit ( $mode )>

Turns on or off auto-commit mode on queries for the database connection.

B<Parameters>

=over

=item I<$mode>

Whether to turn on auto-commit or not.

=back

B<Return Values>

Returns a true value on success, or undef on error.


=item B<$con -E<gt> begin_work ()>

Turns off auto-commit mode for the database connection until transaction
is finished.

B<Return Values>

Returns a true value on success, or undef on error.


=item B<$con -E<gt> commit ()>

Commits the current transaction for the database connection.

B<Return Values>

Returns a true value on success, or undef on error.


=item B<$con -E<gt> rollback ()>

Rollbacks the current transaction for the database.

B<Return Values>

Returns a true value on success, or undef on error.

=back

=head2 Simple Data Fetching

=over 2

=item B<$con -E<gt> selectrow_array ( $statement )>

=item B<$con -E<gt> selectrow_array ( $statement, @bind_values )>

Combines various operations into a single call and returns an array
with values of the first row, or undef on error.
See C<fetchrow_array()> for further information.


=item B<$con -E<gt> selectrow_arrayref ( $statement )>

=item B<$con -E<gt> selectrow_arrayref ( $statement, @bind_values )>

Combines various operations into a single call and returns a reference to an
array with values of the first row, or undef on error.
See C<fetchrow_arrayref()> for further information.


=item B<$con -E<gt> selectrow_hashref ( $statement )>

=item B<$con -E<gt> selectrow_hashref ( $statement, @bind_values )>

Combines various operations into a single call and returns a reference to a
hash with names and values of the first row, or undef on error.
See C<fetchrow_hashref()> for further information.


=item B<$db -E<gt> selectall_arrayref ( $statement )>

=item B<$db -E<gt> selectall_arrayref ( $statement, {} )>

=item B<$db -E<gt> selectall_arrayref ( $statement, undef, @bind_values )>

=item B<$db -E<gt> selectall_arrayref ( $statement, {}, @bind_values )>

Combines various operations into a single call.
See C<fetchall_hashref()> for a description.
Returns undef on error.

=item B<$db -E<gt> selectall_hashref ( $statement, $key )>

=item B<$db -E<gt> selectall_hashref ( $statement, $key, @bind_values )>

Combines various operations into a single call.
See C<fetchall_hashref()> for a description.
Returns undef on error.

=item B<$con -E<gt> select_col ( $statement, @bind_values )>

=item B<$con -E<gt> select_col ( $statement, [$num], @bind_values )>

=item B<$con -E<gt> select_col ( $statement, [\@col], @bind_values )>

=item B<$con -E<gt> select_col ( $statement, [\@col, $num], @bind_values )>

=item B<$con -E<gt> select_col ( $statement, [\%col], @bind_values )>

=item B<$con -E<gt> select_col ( $statement, [\%col, $num_key, $num_val], @bind_values )>

Combines various operations into a single call and returns an array
with values of the first row, or undef on error. The I<@bind_values> are
optional.
See C<fetch_col()> for further information.


=back

=head2 Information and Catalog Functions

=over 2

=item B<DBE -E<gt> open_drivers ()>

Returns a hash of open (loaded) drivers. The key field in the hash contains
the name of the driver and the value field contains a description of the driver.

B<Example>

  %drivers = DBE->open_drivers();


=item B<DBE -E<gt> installed_drivers ()>

Returns a hash of installed drivers. The key field in the hash contains the
name of the driver and the value field contains a description of the driver.

B<Example>

  %drivers = DBE->installed_drivers();


=item B<$con -E<gt> data_sources ()>

Returns a list of data sources (databases) available.


=item B<$con -E<gt> getinfo ( $id )>

Returns general information, (including supported data conversions) about
the DBMS that the application is currently connected to.

B<Example>

  $pattern_escape = $con->getinfo( 14 );

=item B<$con -E<gt> type_info ()>

=item B<$con -E<gt> type_info ( $sql_type )>

Returns a result set that can be used to fetch information about
the data types that are supported by the DBMS.

B<Parameters>

=over

=item I<$sql_type>

The SQL data type being queried as string or number. Supported types are:
(numeric values stay in parenthesis) 

=over

=item * SQL_ALL_TYPES (0)

=item * SQL_BIGINT (-5)

=item * SQL_BINARY (-2)

=item * SQL_VARBINARY (-3)

=item * SQL_CHAR (1)

=item * SQL_DATE (9)

=item * SQL_DECIMAL (3)

=item * SQL_DOUBLE (8)

=item * SQL_FLOAT (6)

=item * SQL_INTEGER (4)

=item * SQL_NUMERIC (2)

=item * SQL_REAL (7)

=item * SQL_SMALLINT (5)

=item * SQL_TIME (10)

=item * SQL_TIMESTAMP (11)

=item * SQL_VARCHAR (12)

=back

If SQL_ALL_TYPES is specified, information about all supported data types
would be returned in ascending order by TYPE_NAME. All unsupported data
types would be absent from the result set.

=back

B<Return Values>

Returns a result set where each table is represented by one row.
The result set contains the columns listed below in the order given.

=over

=item 1 B<TYPE_NAME>

String representation of the SQL data type name.

=item 2 B<DATA_TYPE>

SQL data type code.

=item 3 B<COLUMN_SIZE>

If the data type is a character or binary string, then this column contains
the maximum length in bytes. 

For date, time, timestamp data types, this is the total number of characters
required to display the value when converted to character.

For numeric data types, this is the total number of digits.

=item 4 B<LITERAL_PREFIX>

Character or characters used to prefix a literal; for example, a single
quotation mark (') for character data types or 0x for binary data types;
undef is returned for data types where a literal prefix is not applicable.

=item 5 B<LITERAL_SUFFIX>

Character or characters used to terminate a literal; for example, a single
quotation mark (') for character data types; undef is returned for data types
where a literal suffix is not applicable.

=item 6 B<CREATE_PARAMS>

A list of keywords, separated by commas, corresponding to each parameter that
the application may specify in parentheses when using the name that is
returned in the TYPE_NAME field. The keywords in the list can be any of the
following: length, precision, or scale. They appear in the order that the
syntax requires them to be used. For example, CREATE_PARAMS for DECIMAL
would be "precision,scale"; CREATE_PARAMS for VARCHAR would equal "length."
undef is returned if there are no parameters for the data type definition;
for example, INTEGER.

=item 7 B<NULLABLE>

Whether the data type accepts a NULL value:

0 - if the data type does not accept NULL values.

1 - if the data type accepts NULL values.

2 - if it is not known whether the column accepts NULL values.

=item 8 B<CASE_SENSITIVE>

Indicates whether the data type can be treated as case sensitive for collation
purposes; valid values are TRUE and FALSE.

=item 9 B<SEARCHABLE>

Indicates how the data type is used in a WHERE clause. Valid values are: 

=over

=item SQL_UNSEARCHABLE (0)

The data type cannot be used in a WHERE clause. 

=item SQL_LIKE_ONLY (1)

The data type can be used in a WHERE clause only with the LIKE predicate. 

=item SQL_ALL_EXCEPT_LIKE (2)

The data type can be used in a WHERE clause with all comparison operators
except LIKE. 

=item SQL_SEARCHABLE (3)

The data type can be used in a WHERE clause with any comparison operator.

=back

=item 10 B<UNSIGNED_ATTRIBUTE>

Indicates where the data type is unsigned. The valid values are: TRUE, FALSE
or undef. A undef indicator is returned if this attribute is not applicable
to the data type.

=item 11 B<FIXED_PREC_SCALE>

Contains a TRUE value if the data type is exact numeric and always has the
same precision and scale; otherwise, it contains FALSE.

=item 12 B<AUTO_UNIQUE_VALUE>

Contains TRUE if a column of this data type is automatically set to a unique
value when a row is inserted; otherwise, contains FALSE.

=item 13 B<LOCAL_TYPE_NAME>

Localized version of the data source–dependent name of the data type.
undef is returned if a localized name is not supported by the data source.
This name is intended for display only.

=item 14 B<MINIMUM_SCALE>

The minimum scale of the SQL data type. If a data type has a fixed scale,
the MINIMUM_SCALE and MAXIMUM_SCALE columns both contain the same value.
undef is returned where scale is not applicable.

=item 15 B<MAXIMUM_SCALE>

The maximum scale of the SQL data type. undef is returned where scale is
not applicable. If the maximum scale is not defined separately in the DBMS,
but is defined instead to be the same as the maximum length of the column,
then this column contains the same value as the COLUMN_SIZE column.

=item 16 B<SQL_DATA_TYPE>

The value of the SQL data type as it appears in the SQL_DESC_TYPE field of the
descriptor. This column is the same as the DATA_TYPE column, except for
interval and datetime data types.

For interval and datetime data types, the SQL_DATA_TYPE field in the result
set will return SQL_INTERVAL or SQL_DATETIME, and the SQL_DATETIME_SUB field
will return the subcode for the specific interval or datetime data type.

=item 17 B<SQL_DATETIME_SUB>

When the value of SQL_DATA_TYPE is SQL_DATETIME or SQL_INTERVAL, this column
contains the datetime/interval subcode. For data types other than datetime
and interval, this field is undef.

For interval or datetime data types, the SQL_DATA_TYPE field in the result set
will return SQL_INTERVAL or SQL_DATETIME, and the SQL_DATETIME_SUB field will
return the subcode for the specific interval or datetime data type.

=item 18 B<NUM_PREC_RADIX>

If the data type is an approximate numeric type, this column contains the
value 2 to indicate that COLUMN_SIZE specifies a number of bits. For exact
numeric types, this column contains the value 10 to indicate that
COLUMN_SIZE specifies a number of decimal digits. Otherwise,
this column is NULL.

=item 19 B<INTERVAL_PRECISION>

If the data type is an interval data type, then this column contains the
value of the interval leading precision. Otherwise, this column is NULL.

=back

=item B<$con -E<gt> tables ()>

=item B<$con -E<gt> tables ( $catalog )>

=item B<$con -E<gt> tables ( $catalog, $schema )>

=item B<$con -E<gt> tables ( $catalog, $schema, $table )>

=item B<$con -E<gt> tables ( $catalog, $schema, $table, $type )>

Returns a result set that can be used to fetch information about
tables and views that exist in the database.

B<Parameters>

=over

=item I<$catalog>

String that may contain a pattern-value to qualify the result set.
Catalog is the first part of a three-part table name.

=item I<$schema>

String that may contain a pattern-value to qualify the result set by
schema name.

=item I<$table>

String that may contain a pattern-value to qualify the result set by
table name.

=item I<$type>

String that may contain a value list to qualify the result set by table type. 
The value is a list of values separated by commas for the types of interest.
Valid table type identifiers may include:
ALL, BASE TABLE, TABLE, VIEW, SYSTEM TABLE.
If I<$table> argument is empty, then this is equivalent to specifying all
of the possibilities for the table type identifier.

If SYSTEM TABLE is specified, then both system tables and system views
(if there are any) are returned.

=back

B<Note>

Since "_" is a pattern matching value, you may need to escape
pattern-values by C<escape_pattern()> first.

B<Return Values>

Returns a result set where each table is represented by one row.
The result set contains the columns listed below in the order given.

=over

=item 1 B<TABLE_CAT>

The catalog identifier.

=item 2 B<TABLE_SCHEM>

The name of the schema containing TABLE_NAME.

=item 3 B<TABLE_NAME>

The name of the table, or view, or alias, or synonym.

=item 4 B<TABLE_TYPE>

Identifies the type given by the name in the TABLE_NAME column.
It can have the string values 'TABLE', 'VIEW', 'BASE TABLE', or 'SYSTEM TABLE'.

=item 5 B<REMARKS>

Contains the descriptive information about the table.

=back

=item B<$con -E<gt> columns ()>

=item B<$con -E<gt> columns ( $catalog )>

=item B<$con -E<gt> columns ( $catalog, $schema )>

=item B<$con -E<gt> columns ( $catalog, $schema, $table )>

=item B<$con -E<gt> columns ( $catalog, $schema, $table, $column )>

Get a list of columns in the specified tables.
The information is returned in a result set, which can be retrieved using
the same functions that are used to fetch a result set generated by a
SELECT statement.

B<Parameters>

=over

=item I<$catalog>

String that may contain a pattern-value to qualify the result set.
Catalog is the first part of a three-part table name. 

=item I<$schema>

String that may contain a pattern-value to qualify the result set by
schema name.

=item I<$table>

String that may contain a pattern-value to qualify the result set by
table name.

=item I<$column>

String that may contain a pattern-value to qualify the result set by
column name.

=back

B<Note>

Since "_" is a pattern matching value, you may need to escape
pattern-values by C<escape_pattern()> first.

B<Return Values>

Returns a result set class, with columns listed below in the order given.

=over

=item 1 B<TABLE_CAT>

The catalog identifier.

=item 2 B<TABLE_SCHEM>

The name of the schema containing TABLE_NAME.

=item 3 B<TABLE_NAME>

The name of the table or view.

=item 4 B<COLUMN_NAME>

Column identifier. Name of the column of the specified table or view.

=item 5 B<DATA_TYPE>

Identifies the SQL data type of the column.

=item 6 B<TYPE_NAME>

String representing the name of the data type corresponding to DATA_TYPE.

=item 7 B<COLUMN_SIZE>

Defines the maximum length in characters for character data
types, the number of digits for numeric data types or the length in
the representation of temporal types.

=item 8 B<BUFFER_LENGTH>

The length in bytes of transferred data.

=item 9 B<DECIMAL_DIGITS>

The total number of significant digits to the right of the decimal point.

=item 10 B<NUM_PREC_RADIX>

The radix for numeric precision. The value is 10 or 2 for numeric data types
and NULL (undef) if not applicable.

=item 11 B<NULLABLE>

FALSE if the column does not accept NULL values, or TRUE if the column
accepts NULL values, or undef if unknown.

=item 12 B<REMARKS>

May contain descriptive information about the column.

=item 13 B<COLUMN_DEF>

The default value of the column. The value in this column should be
interpreted as a string if it is enclosed in quotation marks.

=item 14 B<SQL_DATA_TYPE>

This column is the same as the DATA_TYPE column, except for datetime and
interval data types. 

=item 15 B<SQL_DATETIME_SUB>

The subtype code for datetime and interval data types. For other data types,
this column returns a NULL.

=item 16 B<CHAR_OCTET_LENGTH>

The maximum length in bytes of a character or binary data type column.
For all other data types, this column returns a NULL.

=item 17 B<ORDINAL_POSITION>

The ordinal position of the column in the table. The first column in the
table is number 1.

=item 18 B<IS_NULLABLE>

"NO" if the column does not include NULLs.

"YES" if the column could include NULLs.

This column returns a zero-length string if nullability is unknown. 

=back


=item B<$con -E<gt> primary_keys ( $catalog, $schema, $table )>

Returns a list of column names that comprise the primary key for a table.
The information is returned in a result set, which can be retrieved using
the same functions that are used to process a result set that is generated
by a query.

B<Parameters>

=over

=item I<$catalog>

Catalog qualifier of a 3 part table name.

=item I<$schema>

Schema qualifier of table name.

=item I<$table>

Table name.

=back

B<Return Values>

Returns the primary key columns from a single table, Search patterns cannot
be used to specify the schema qualifier or the table name.

The result set contains the columns that are listed below, ordered by
TABLE_CAT, TABLE_SCHEM, TABLE_NAME, and ORDINAL_POSITION.

=over

=item 1 B<TABLE_CAT>

The catalog identifier.

=item 2 B<TABLE_SCHEM>

The name of the schema containing TABLE_NAME.

=item 3 B<TABLE_NAME>

Name of the specified table.

=item 4 B<COLUMN_NAME>

Primary Key column name.

=item 5 B<KEY_SEQ>

Column sequence number in the primary key, starting with 1.

=item 6 B<PK_NAME>

Primary key identifier. NULL if not applicable to the data source.

=back

=item B<$con -E<gt> foreign_keys ( $pk_cat, $pk_schem, $pk_table, $fk_cat,
                             $fk_schem, $fk_table )>

Returns information about foreign keys for the specified table.
The information is returned in a result set which can be processed
using the same functions that are used to retrieve a result that is
generated by a query.

B<Parameters>

=over

=item I<$pk_cat>

Catalog qualifier of the primary key table.

=item I<$pk_schem>

Schema qualifier of the primary key table.

=item I<$pk_table>

Name of the table name containing the primary key.

=item I<$fk_cat>

Catalog qualifier of the table containing the foreign key.

=item I<$fk_schem>

Schema qualifier of the table containing the foreign key.

=item I<$fk_table>

Name of the table containing the foreign key.

=back

B<Usage>

If I<$pk_table> contains a table name, and I<$fk_table> is empty,
I<foreign_keys()> returns a result set that contains the primary key
of the specified table and all of the foreign keys (in other tables)
that refer to it.

If I<$fk_table> contains a table name, and I<$pk_table> is empty,
I<foreign_keys()> returns a result set that contains all of the
foreign keys in the specified table and the primary keys
(in other tables) to which they refer.

If both I<$pk_table> and I<$fk_table> contain table names,
I<foreign_keys()> returns the foreign keys in the table specified
in I<$fk_table> that refer to the primary key of the table
specified in I<$pk_table>. This should be one key at the most.


B<Return Values>

Returns a reference to a result set, or FALSE on failure.
The columns of the result set are listed below in the order given.

=over

=item 1 B<PKTABLE_CAT>

Primary key table catalog name; NULL if not applicable to the data source.

=item 2 B<PKTABLE_SCHEM>

Primary key table schema name; NULL if not applicable to the data source.

=item 3 B<PKTABLE_NAME>

Primary key table name.

=item 3 B<PKCOLUMN_NAME>

Primary key column name. The driver returns an empty string for a column
that does not have a name.

=item 4 B<FKTABLE_CAT>

Foreign key table catalog name; NULL if not applicable to the data source.

=item 5 B<FKTABLE_SCHEM>

Foreign key table schema name; NULL if not applicable to the data source.

=item 6 B<FKTABLE_NAME>

Foreign key table name.

=item 7 B<FKCOLUMN_NAME>

Foreign key column name. The driver returns an empty string for a column
that does not have a name.

=item 8 B<KEY_SEQ>

The ordinal position of the column in the key, starting at 1.

=item 9 B<UPDATE_RULE>

Action to be applied to the foreign key when the SQL operation is UPDATE:

  0  -  CASCADE
  1  -  RESTRICT
  2  -  SET NULL
  3  -  NO ACTION
  4  -  SET DEFAULT

=item 10 B<DELETE_RULE>

Action to be applied to the foreign key when the SQL operation is DELETE:

  0  -  CASCADE
  1  -  RESTRICT
  2  -  SET NULL
  3  -  NO ACTION
  4  -  SET DEFAULT

=item 11 B<FK_NAME>

Foreign key name. NULL if not applicable to the data source.

=item 12 B<PK_NAME>

Primary key name. NULL if not applicable to the data source.

=back


=item B<$con -E<gt> statistics ( $catalog, $schema, $table )>

=item B<$con -E<gt> statistics ( $catalog, $schema, $table, $unique_only )>

Retrieve a list of statistics about a single table and the indexes associated
with the table.
The information is returned in a result set, which can be retrieved using
the same functions that are used to fetch a result set generated by a
SELECT statement.

B<Parameters>

=over

=item I<$catalog>

Catalog is the first part of a three-part table name. It cannot contain a
string search pattern.

=item I<$schema>

Schema qualifier of the specified table. It cannot contain a string search
pattern.

=item I<$table>

Table name. This argument cannot be a empty. It cannot contain a string
search pattern.

=item I<$unique_only>

If TRUE, only unique indexes are returned. If FALSE, all indexes are
returned.

=back

B<Return Values>

Returns a result set class, with columns listed below in the order given.

=over

=item 1 B<TABLE_CAT>

Catalog name of the table to which the statistic or index applies; NULL if
not applicable to the data source.

=item 2 B<TABLE_SCHEM>

Schema name of the table to which the statistic or index applies; NULL if
not applicable to the data source.

=item 3 B<TABLE_NAME>

Table name of the table to which the statistic or index applies.

=item 4 B<NON_UNIQUE>

Indicates whether the index does not allow duplicate values:

TRUE if the index values can be nonunique.

FALSE if the index values must be unique.

NULL is returned if TYPE is 'table'.

=item 5 B<INDEX_QUALIFIER>

The identifier that is used to qualify the index name doing a DROP INDEX;
NULL is returned if an index qualifier is not supported by the data source
or if TYPE is 'table'.

=item 6 B<INDEX_NAME>

Index name; NULL is returned if TYPE is 'table'.

=item 7 B<TYPE>

Type of information being returned:

=for formatter none

  'table'      indicates a statistic for the table (in the CARDINALITY
               or PAGES column). 
  'btree'      indicates a B-Tree index.
  'clustered'  indicates a clustered index. 
  'content'    indicates a content index.
  'hashed'     indicates a hashed index.
  'other'      indicates another type of index.

=for formatter perl

=item 8 B<ORDINAL_POSITION>

Column sequence number in index (starting with 1); NULL is returned if
TYPE is 'table'.

=item 9 B<COLUMN_NAME>

Column name. If the column is based on an expression, such as
SALARY + BENEFITS, the expression is returned; if the expression cannot
be determined, an empty string is returned. 

=item 10 B<ASC_OR_DESC>

Sort sequence for the column: "A" for ascending; "D" for descending;
NULL is returned if column sort sequence is not supported by the
data source or if TYPE is 'table'.

=item 11 B<CARDINALITY>

Cardinality of table or index; number of rows in table if TYPE is 'table';
number of unique values in the index if TYPE is not 'table';
NULL is returned if the value is not available from the data source.

=item 12 B<PAGES>

Number of pages used to store the index or table; number of pages for
the table if TYPE is 'table'; number of pages for the index if TYPE
is not 'table'; NULL is returned if the value is not available from
the data source or if not applicable to the data source.

=back

=item B<$con -E<gt> special_columns ( $type, $catalog, $schema, $table )>

=item B<$con -E<gt> special_columns ( $type, $catalog, $schema, $table, $scope )>

=item B<$con -E<gt> special_columns ( $type, $catalog, $schema, $table, $scope, $nullable )>

I<special_columns> retrieves the following information about columns within a
specified table:

=over

=item * The optimal set of columns that uniquely identifies a row in the table.

=item * Columns that are automatically updated when any value in the row is
updated by a transaction.

=back

The information is returned in a result set, which can be retrieved using
the same functions that are used to fetch a result set generated by a
SELECT statement.

B<Parameters>

=over

=item I<$type> (integer)

Type of column to return. Must be one of the following values:

=over

=item SQL_BEST_ROWID (1)

Returns the optimal column or set of columns that, by retrieving values
from the column or columns, allows any row in the specified table to be
uniquely identified. A column can be either a pseudo-column specifically
designed for this purpose (as in Oracle ROWID or Ingres TID) or the column
or columns of any unique index for the table. 

=item SQL_ROWVER (2)

Returns the column or columns in the specified table, if any, that are
automatically updated by the data source when any value in the row is
updated by any transaction (as in SQLBase ROWID or Sybase TIMESTAMP).

=back

=item I<$catalog>

Catalog is the first part of a three-part table name. It cannot contain a
string search pattern.

=item I<$schema>

Schema qualifier of the specified table. It cannot contain a string search
pattern.

=item I<$table>

Table name. This argument cannot be a empty. It cannot contain a string
search pattern.

=item I<$scope> (integer)

Minimum required duration for which the unique row identifier is valid. 
I<$scope> must be one of the following:

=over

=item SQL_SCOPE_CURROW (0)

The row identifier is guaranteed to be valid only while positioned on that
row. A later reselect using the same row identifier values may not return
a row if the row was updated or deleted by another transaction. 

=item SQL_SCOPE_TRANSACTION (1)

The row identifier is guaranteed to be valid for the duration of the
current transaction. 

=item SQL_SCOPE_SESSION (2)

The row identifier is guaranteed to be valid for the duration of the
connection.

=back

=item I<$nullable> (integer)

Determines whether to return special columns that can have a NULL value.
Must be one of the following:

=over

=item SQL_NO_NULLS (0)

Exclude special columns that can have NULL values. Some drivers cannot
support SQL_NO_NULLS, and these drivers will return an empty result set if
SQL_NO_NULLS was specified. Applications should be prepared for this case
and request SQL_NO_NULLS only if it is absolutely required. 

=item SQL_NULLABLE (1)

Return special columns even if they can have NULL values.

=back

=back

B<Return Values>

Returns a result set class, with columns listed below in the order given.

=over

=item 1 B<SCOPE>

Actual scope of the rowid. Contains one of the following values:

SQL_SCOPE_CURROW (0), SQL_SCOPE_TRANSACTION (1), SQL_SCOPE_SESSION (2)

NULL is returned when I<$type> is SQL_ROWVER (2). For a description of
each value, see the description of Scope in "Parameters," earlier in
this section.

=item 2 B<COLUMN_NAME>

Column name. The driver returns an empty string for a column that does not
have a name.

=item 3 B<DATA_TYPE>

SQL data type of the column.

=item 4 B<TYPE_NAME>

DBMS character string represented of the name associated with DATA_TYPE
column value.

=item 5 B<COLUMN_SIZE>

The size of the column on the data source.

=item 6 B<BUFFER_LENGTH>

The length, in bytes, of the data returned in the default C type.

=item 7 B<DECIMAL_DIGITS>

The decimal digits of the column on the data source. NULL is returned for
data types where decimal digits are not applicable.

=item 8 B<PSEUDO_COLUMN>

Indicates whether the column is a pseudo-column, such as Oracle ROWID:

SQL_PC_UNKNOWN (0), SQL_PC_NOT_PSEUDO (1), SQL_PC_PSEUDO (2)

=back

=back

=head2 Other Functions

=over 2

=item B<$con -E<gt> escape_pattern ( $string )>

Escape pattern matching characters for use in C<tables()> and C<columns()>.

B<Example>

  # since "_" matches any single character,
  # it must be escaped to match it as "_"
  $res = $con->tables( $con->escape_pattern( 'Database_Name' ) );

=item B<$con -E<gt> quote ( $value )>

Quote a value for use as a literal value in an SQL statement,
by escaping any special characters (such as quotation marks) contained within
the string and adding the required type of outer quotation marks.

The I<quote()> method should not be used with placeholders.

B<Parameters>

=over

=item I<$value>

Value to be quoted.

=back

B<Return Values>

The quoted value with adding the required type of outer quotation marks.

B<Example>

  print $con->quote( "Don't clash with quote chars" );
  # output: 'Don''t clash with quote chars'


=item B<$con -E<gt> quote_bin ( $value )>

Quote a value for use as a binary value in an SQL statement,

The I<quote_bin()> method should not be used with placeholders.

B<Parameters>

=over

=item I<$value>

Value to be quoted as binary value.

=back

B<Return Values>

The quoted value to use as binary value in SQL statements, or undef if the
driver does not support binary values directly in SQL statements.

B<Example>

  print $con->quote_bin( "DBE" );
  
  # SQLite3:      X'444245'
  # MySQL:        0x444245
  # PostgreSQL:   '\104\102\105'


=item B<$con -E<gt> quote_id ( $field )>

=item B<$con -E<gt> quote_id ( $table, $field )>

=item B<$con -E<gt> quote_id ( $catalog, $schema, $table )>

Quote an identifier (table name etc.) for use in an SQL statement, by escaping
any special characters it contains and adding the required type of outer
quotation marks.

B<Parameters>

One or more identifiers to quote.

B<Return Values>

The quoted string with adding the required type of outer quotation marks.

B<Examples>

  # MySQL driver:
  
  $s = $con->quote_id( 'table' );
  # returns: `table`
  
  $s = $con->quote_id( 'table', 'field' );
  # returns: `table`.`field`
  
  $s = $con->quote_id( 'table', '*' );
  # returns: `table`.*


=item B<$con -E<gt> name_convert ()>

=item B<$con -E<gt> name_convert ( $name )>

Sets or returns the name conversion in result sets for subsequent queries.

B<Examples>

  # read the actual state
  print "current name conversion: ", $con->name_convert() || "none", "\n";
  
  # convert names to uppercase
  $con->name_convert( "NAME_uc" );
  # run a query
  $res = $con->query( "SELECT ..." );
  $res->names; # uppercase
  
  # convert names to lowercase
  $con->name_convert( "lc" );
  # run a query
  $res = $con->query( "SELECT ..." );
  $res->fetchrow_hashref; # keys are in lowercase
  
  # disable name conversion
  $con->name_convert( "" );


=item B<$con -E<gt> row_limit ()>

=item B<$con -E<gt> row_limit ( $limit )>

=item B<$con -E<gt> row_limit ( $limit, $offset )>

Adds a limit to SELECT statements. Setting this option affects subsequent
calls to C<query()> and C<prepare()>.
The driver holds I<$limit> rows in the result set and calls internally
SELECT again to give access to all rows.

B<Examples>

I<Table "t1" should contain more then one row>

  # set limit to 1 row
  $con->row_limit( 1 );
  
  # the driver holds one row in the result set
  $res = $con->query( "SELECT * FROM t1" );
  
  # num_rows returns the total number of rows available
  print $res->num_rows, "\n";
  
  # fetch the first row
  @row = $res->fetch_row;
  
  # fetch the second row
  # the driver now makes a new SELECT call to the dbms
  @row = $res->fetch_row;
  
  # disable row limit for further queries
  $con->row_limit( 0 );

I<Pagination Example>

  $limit = 10;
  $offset = 0;
  
  $con->row_limit( $limit, $offset );
  $res->query( "SELECT * FROM t1" );
  
  $first_row = $offset + 1;
  $last_row = $res->num_rows - $offset;
  $last_row > $limit and $last_row = $limit;
  
  print "print rows $first_row to $last_row\n";
  for $row_pos( $first_row .. $last_row ) {
      $res->fetch_row( \@row ) or last;
      print "row $row_pos ", join( '|', @row ), "\n";
  }

B<Note>

I<row_limit()> must be supported by the driver. Known supporting drivers are
MySQL, PostgreSQL and SQLite3.


=item B<$con -E<gt> attr_set ( $name, $value )>

=item B<$con -E<gt> attr_get ( $name )>

Sets or gets a connection attribute. Attribute names are given as string.
Following attributes are available to DBE:

=for formatter none

  croak, warn, reconnect

=for formatter perl

A description of these attributes can be found on C<connect()>.

B<Example>

  $prv = $con->get_attr( 'croak' );
  # enable croak on error
  $con->set_attr( 'croak' => 1 );
  eval {
      # raise an error
      $con->do( "SELECT" );
  };
  # restore previous attribute
  $con->set_attr( 'croak' => $prv );
  if( $@ ) {
      print "SELECT failed: $@\n";
  }

=item B<$con -E<gt> ping ()>

Determines if the connection is still working.
Returns TRUE on success, or FALSE on failure.

=item B<$con -E<gt> driver_has ( $fnc_name )>

Checks the driver for a function.

B<Parameters>

=over

=item I<$fnc_name>

Name of the function.

=back

B<Return Values>

Return TRUE if the function exists, or FALSE if not.

B<Example>

  if( ! $con->driver_has( 'prepare' ) ) {
      print "statements are not available!\n";
  }

=back

=head2 Large Objects

Drivers with access to large objects would create this class which supports
following methods.

=over

=item B<$lob -E<gt> read ( $buffer, $length )>

Reads a maximum of length bytes from the large object.

B<Parameters>

=over

=item I<$buffer>

A variable to write the read bytes into.

=item I<$length>

The maximum number of bytes read is specified by the length parameter.

=back

B<Return Values>

Returns number of bytes read, 0 on eof, or undef on error.


=item B<$lob -E<gt> write ( $buffer )>

=item B<$lob -E<gt> write ( $buffer, $offset )>

=item B<$lob -E<gt> write ( $buffer, $offset, $length )>

Writes to the large object from the given buffer.

B<Parameters>

=over

=item I<$buffer>

The buffer to be written. 

=item I<$offset>

The optional parameter I<$offset> can specify an alternate offset in the
buffer.

=item I<$length>

The optional parameter I<$length> can specify an alternate length of bytes
written to the socket. If this length is greater then the buffer length,
it is silently truncated to the length of the buffer. 

=back

B<Return Values>

Returns the number of bytes successfully written, or undef on error.


=item B<$lob -E<gt> size ()>

Returns the size of the large object, or undef on error.


=item B<$lob -E<gt> seek ( $position, $origin )>

Moves the large object pointer to a specified location.

B<Parameters>

=over

=item I<$position>

Number of bytes or characters from I<$origin>.

=item I<$origin>

Initial position. It must be one of the following values:

=for formatter none

  0 - Beginning of large object. 
  1 - Current position of large object. 
  2 - End of large object. 

=for formatter perl

=back

B<Return Values>

Returns a true value on success, or undef on error.


=item B<$lob -E<gt> tell ()>

Returns the current position of the large object, or undef on error.


=item B<$lob -E<gt> print ( ... )>

Writes to the large object from the given parameters.

B<Return Values>

Returns a true value on success, or undef on error.

B<Example>

  $lob->print( 'hello lob', "\n" );


=item B<$lob -E<gt> getc ()>

Returns the character read as an integer, or -1 on end of large object,
or undef on error.


=item B<$lob -E<gt> eof ()>

Returns a true value if a read operation has attempted to read past the end
of the large object, 0 otherwise, or undef on error.


=item B<$lob -E<gt> close ()>

Closes the large object stream.

B<Return Values>

Returns a true value on success, or undef on error.

=back

=head1 THREADSAFETY

DBE was build for threads and should run properly with threads enabled.
Objects are shared between threads. This means not that database
connections are thread safe. It is never safe to use the same connection in
two or more threads. You should make own connections for each thread.
If you want share a database connection between threads you should use
the I<lock()> statement, which is a part of the L<threads::shared> module.


=head1 AUTHORS

Navalla org., Christian Mueller, L<http://www.navalla.org/>

=head1 COPYRIGHT

The DBE module is free software. You may distribute under the terms of
either the GNU General Public License or the Artistic License, as specified in
the Perl README file.

=cut
