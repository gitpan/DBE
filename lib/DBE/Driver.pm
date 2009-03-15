package DBE::Driver;

our( $VERSION );

BEGIN {
	$VERSION = '0.99_1';
	my( $pn );
	$pn = '/auto/DBE';
	foreach( reverse @INC ) {
		if( -f $_ . $pn . '/dbe.h' ) {
			$ENV{'DBE_INC'} = '-I' . $_ . $pn;
			last;
		}
	}
	$ENV{'DBE_INC'} or die "DBE header file not found!";
}

1;

__END__

=head1 NAME

DBE::Driver - A Guide for creating DBE Drivers

=for formatter perl

=head1 DESCRIPTION

This document is a guide for creating a driver for the DBE module. DBE
drivers are mainly written in C. A XS function replaces the important
data once. Experience with C, Perl, XS and the ExtUtils::MakeMaker module
is a prerequisite for succesfully creating a driver.

=head2 Driver Layout

DBE communicates to the driver through a structure I<drv_def_t>. The driver
communicates to DBE through a structure I<dbe_t>. Both structures are defined
in the header file I<dbe.h>, which is installed with DBE.
DBE defines structure types I<dbe_con_t>, I<dbe_res_t> and I<dbe_stmt_t>, that
are unknown to the driver. In the opposite side DBE uses prototypes to
structures which the driver implements. These structures are I<drv_con_t>,
I<drv_res_t> and I<drv_stmt_t>.

The following part shows the design of a driver named B<Dummy>.

B<Required Files>

=over

=item * Makefile.PL

=item * README

=item * MANIFEST

=item * DUMMY.pm

=item * DUMMY.xs

=item * dbe_dummy.h

=item * dbe_dummy.c

=item * t/*.t

=back

=head3 [ Makefile.PL ]

  use ExtUtils::MakeMaker;
  # use DBE::Driver to get the include settings
  use DBE::Driver;
  
  WriteMakefile(
      NAME => 'DBE::Driver::DUMMY',
      VERSION_FROM => 'DUMMY.pm',
      # dbe include settings
      INC => $ENV{'DBE_INC'},
      LIB => [],
      OBJECT => '$(O_FILES)',
      XS => {'DUMMY.xs' => 'DUMMY.c'},
      C => ['dummy_dbe.c', 'DUMMY.c'],
      H => ['dummy_dbe.h'],
  );

The Perl Makefile Script for the C<ExtUtils::MakeMaker> Module.


=head3 [ README ]

=for formatter none

  DBE::Driver::DUMMY
  =====================
  
  Driver description
  
  INSTALLATION
  
  To install this library type the following:
  
     perl Makefile.PL
     make
     make test
     make install
  
  COPYRIGHT AND LICENCE
  
  Copyright (C) [author]
  
  The library is free software. You may redistribute and/or modify under
  the terms of either the GNU General Public License or the Artistic
  License, as specified in the Perl README file.

=for formatter perl

The README file contains important information, like driver description,
installation instruction and copyright.


=head3 [ MANIFEST ]

=for formatter none

  Makefile.PL
  MANIFEST
  README
  DUMMY.pm
  DUMMY.xs
  dummy_dbe.h
  dummy_dbe.c
  t/...                 test files

=for formatter perl

The MANIFEST is used to distribute the driver with the command "make dist".
It contains the names of the files needed. Each filename must stay on one line. 
Comments can be added right to the filename separated by whitespace characters.


=head3 [ DUMMY.pm ]

  package DBE::Driver::DUMMY;
  
  our $VERSION;
  
  BEGIN {
      $VERSION = '1.0.0';
      require XSLoader;
      XSLoader::load(__PACKAGE__, $VERSION);
  }

I<DUMMY.pm> is the first place of contact with Perl. There will be the version
of the driver defined and the Perl XS module loaded.

B<Note:> The name of the module must be written in upper case characters.

=head3 [ DUMMY.xs ]

=for formatter cpp


  #include "dummy_dbe.h"
  
  MODULE = DBE::Driver::DUMMY        PACKAGE = DBE::Driver::DUMMY
  
  void
  init(dbe, drv)
      void *dbe;
      void *drv;
  PPCODE:
      /* set the pointer to dbe_t */
      g_dbe = (const dbe_t *) dbe;
      /* return a pointer to the dummy driver structure */
      XSRETURN_IV(PTR2IV(&g_dbe_drv));

=for formatter perl

The Perl XS module implements a function I<init>. This function will be called
once by DBE. The first parameter is a pointer to the structure I<dbe_t>. The
second parameter is a pointer to the dbe driver structure I<dbe_drv_t>, which
is unknown to the driver and can be mostly ignored.
The function must return a pointer to the driver definition structure
I<drv_def_t>.

B<Note:> The name of the module must be written in upper case characters.


=head3 [ dummy_dbe.h ]

=for formatter cpp

  /* include the dbe header file */
  #include <dbe.h>
  
  /* pointer to the dbe structure, see dbe.h */
  extern const dbe_t *g_dbe;
  
  /* driver definition structure, see dbe.h */
  extern const drv_def_t g_drv_def;

=for formatter perl


The header file includes I<dbe.h> and prototypes the two global variables
I<dbe_t *> and I<drv_def_t> which are used in the I<init> function in the Perl
XS module.


=head3 [ dummy_dbe.c ]

=for formatter cpp

  #include "dummy_dbe.h"
  
  /* global pointer to dbe_t */
  const dbe_t *g_dbe;
  
  /* implementation of the driver starts here */
  
  drv_con_t *dummy_drv_connect (dbe_con_t *dbe_con, char **args, int argc)
  {
      g_dbe->set_error(dbe_con, -1, "Not yet implemented");
      return NULL;
  }
  
  ...
  
  /* fill drv_def_t with our driver */
  
  const drv_def_t g_drv_def =
  {
      /* dbe version */
      DBE_VERSION,
      /* description */
      "Dummy DBE Driver " XS_VERSION,
      /* functions */
      NULL, /* mem_free */
      NULL, /* drv_free */
      dummy_drv_connect,
      ...
  };

=for formatter perl

The C file implements the driver functions and fills it into the structure
I<drv_def_t>. It also defines a global pointer to structure I<dbe_t>.

=head3 [ t/*.t ]

Directory containing test scripts. Please see C<ExtUtils::MakeMaker> for
details.

=head2 Functions provided by DBE

Functions provided by DBE are defined in I<dbe_t>.

=head3 The dbe_t datatype

The B<dbe_t> type defines a structure B<struct st_dbe>. The elements of
I<st_dbe> are explained below.

=over

=item const int B<st_dbe::version>

The installed version of DBE.

=item void (*B<st_dbe::dbe_set_error>)
    ( dbe_con_t *dbe_con, int code, const char *msg, ... ) 

Sets an error code and message to the given connection handle. Additional
arguments can be passed to complete I<msg> with vsprintf.

B<Example>

=for formatter cpp

  /* set an error */
  g_dbe->dbe_set_error(
      dbe_con, errno, "Error: table '%s' does not exist", tablename
  );
  
  /* clear the error */
  g_dbe->dbe_set_error(dbe_con, 0, NULL);

=for formatter perl

=item void (*B<st_dbe::trace>)
   ( dbe_con_t *dbe_con, int level, int show_caller, const char *msg, ... )

Trace a message with specified level. Available levels are:

=for formatter none

  DBE_TRACE_NONE            do not trace
  DBE_TRACE_SQL             trace sql statements
  DBE_TRACE_SQLFULL         trace sql functions around statements
  DBE_TRACE_ALL             trace all

=for formatter perl

B<Example>

=for formatter cpp

  /* without caller */
  g_dbe->trace(dbe_con, DBE_TRACE_ALL, FALSE, "Connected to %s", hostname);
  /* output:  Connected to [hostname] */
  
  /* or with caller */
  g_dbe->trace(dbe_con, DBE_TRACE_SQL, TRUE, "sql(%s)", statement);
  /* output:  sql([statement]) at [file] line [line] */

=for formatter perl

=item void (*B<st_dbe::set_base_class>)
  ( dbe_drv_t *dbe_drv, enum dbe_object obj, const char *classname )

Overwrites the default class names of connection, result set or statement
objects.
Default classes are DBE::CON, DBE::RES and DBE::STMT.

=item void *(*B<st_dbe::get_object>) ( enum dbe_object obj, SV *this )

Retrieves a pointer to a driver object. Can be used in XS functions within an
overloaded class.

B<Example of a sub classed connection object>

[DUMMY.XS]

=for formatter cpp

  MODULE = DBE::Driver::DUMMY        PACKAGE = DBE::Driver::DUMMY
  
  void
  init(dbe, drv)
      void *dbe;
      void *drv;
  PPCODE:
      g_dbe = (const dbe_t *) dbe;
      
      /* set connection class to DBE::CON::DUMMY */
      g_dbe->set_base_class(
          (dbe_drv_t *) drv, DBE_OBJECT_CON, "DBE::CON::DUMMY");
      
      XSRETURN_IV(PTR2IV(&g_dbe_drv));
  
  
  MODULE = DBE::Driver::DUMMY        PACKAGE = DBE::CON::DUMMY
  
  BOOT:
  {
      /* inherit DBE::CON */
      AV *isa = get_av("DBE::CON::DUMMY::ISA", TRUE);
      av_push(isa, newSVpvn("DBE::CON", 8)); 
  }
  
  void
  test(this)
      SV *this;
  PREINIT:
      drv_con_t *con;
  PPCODE:
      con = (drv_con_t *) g_dbe->get_object(DBE_OBJECT_CON, this);
      if (con == NULL)
          XSRETURN_EMPTY;
      /* do something with the connection */
      Perl_warn(aTHX_ "test was succesful");
      XSRETURN_YES;

=for formatter perl

call the new function

  $con = DBE->connect('driver' => 'dummy');
  $con->test();


=item void *(*B<st_dbe::handle_error_xs>)
	( dbe_con_t *dbe_con, enum dbe_object obj, SV *this, const char *action )

Starts the error handler from you XS function.

B<Example>

=for formatter cpp

  void
  test(this)
      SV *this;
  PREINIT:
      drv_con_t *con;
  PPCODE:
      con = (drv_con_t *) g_dbe->get_object(DBE_OBJECT_CON, this);
      if (con == NULL)
          XSRETURN_EMPTY;
      g_dbe->set_error(con->dbe_con, -1, "test error message");
      g_dbe->handle_error_xs(con->dbe_con, DBE_OBJECT_CON, this, "test");
      // code here would be unreachable

=for formatter perl


=item char *(*B<st_dbe::error_str>) ( int errnum, char *buf, size_t buflen )

Fills the error message of a system error into I<buf>.
Returns the pointer to I<buf> on success or NULL on error

=back

=head3 Driver Result Sets

=over

=item dbe_res_t *(*B<st_dbe::alloc_result>)
    ( dbe_con_t *dbe_con, drv_res_t *drv_res, u_long num_fields )

Allocates a dbe result set with a driver result set.

=item void (*B<st_dbe::free_result>) ( dbe_res_t *dbe_res )

Frees a dbe result set that was allocated by alloc_result()

=back

=head3 Virtual Result Sets

=over

=item dbe_res_t *(*B<st_dbe::vres_create>) ( dbe_con_t *dbe_con )

Creates a virtual result set and returns a pointer to the dbe result set.

=item void (*B<st_dbe::vres_free>) ( dbe_res_t *dbe_res )

Frees a virtual result set.

=item int (*B<st_dbe::vres_add_column>)
    ( dbe_res_t *dbe_res, const char *name, size_t name_length,
      enum dbe_type type )

Adds a field to a virtual result set. I<enum dbe_type> is defined in I<dbe.h>.

=item int (*B<st_dbe::vres_add_row>) ( dbe_res_t *dbe_res, dbe_str_t *data )  

Adds a row to a virtual result set; the row data must be conform
to the column types. NULL pointer in the I<value> part of I<dbe_str_t>
define NULL values.

=over

=item * DBE_TYPE_INTEGER points to int

=item * DBE_TYPE_DOUBLE points to double

=item * DBE_TYPE_BIGINT points to xlong

=item * DBE_TYPE_VARCHAR points to char

=item * DBE_TYPE_BINARY points to void

=back

=back

B<Example of a Virtual Result set>

=for formatter cpp

  dbe_res_t *vrt_res;
  dbe_str_t data[2];
  int id;
  
  /* create a virtual result set
     'dbe_con' is given by 'st_drv_def::drv_connect' */
  vrt_res = g_dbe->vres_create(dbe_con);
  
  /* add two columns to the virtual result set */
  g_dbe->vres_add_column(vrt_res, "ID", 2, DBE_TYPE_INTEGER);
  g_dbe->vres_add_column(vrt_res, "NAME", 4, DBE_TYPE_VARCHAR);
  
  /* set the data for the first row */
  /* first field points to int */
  data[0].value = (char *) &id;
  id = 1;
  /* second field points to char */
  data[1].value = "Anderson";
  data[1].length = 8;
  
  /* add the row to the virtual result set */
  g_dbe->vres_add_row(vrt_res, data);

=for formatter perl


=head3 Search Patterns and Regular Expressions

=over

=item regexp* (*B<st_dbe::regexp_from_string>)
              ( const char *str, size_t str_len ) 

Create a Perl regular expression from a string. The expression string must be
enclosed by tags and can contain additional options. (i.e. B<"/^table_*/i">)

=item regexp* (*B<st_dbe::regexp_from_pattern>)
              ( const char *pattern, size_t pattern_len, int force ) 

Converts a SQL search pattern to a Perl regular expression
(i.e. pattern B<"%FOO\_BAR_"> to regexp B</.*?FOO_BAR./>).

Returns NULL if I<pattern> contains no search parameters and
I<force> was set to FALSE

=item int (*B<st_dbe::regexp_match>) ( regexp *re, char *str, size_t str_len )

Counts the matches of a regular expression on I<str> and returns the
number of hits.

=item void (*B<st_dbe::regexp_free>) ( regexp *re )

Frees a Perl regexp.

=item size_t (*B<st_dbe::unescape_pattern>)
              ( char *str, size_t str_len, const char esc )

Unescapes a search pattern (i.e. B<table\_name> to B<table_name>) within the
string an returns the resulting size. Parameter I<esc> defines the escape
character.

=item SV *(*B<st_dbe::dbe_escape_pattern>)
    ( const char *str, size_t str_len, const char esc )

Escapes a search pattern (i.e B<table_name> to B<table\_name>) and returns
the result string as SV. Parameter I<esc> defines the escape
character.

=back

=head3 Unicode functions

=over

=item unsigned int (*B<st_dbe::towupper>) ( unsigned int c )

Converts an unicode character (widechar/utf-16be) to upper case.

=item unsigned int (*B<st_dbe::towlower>) ( unsigned int c )

Converts an unicode character (widechar/utf-16be) to lower case.

=item int (*B<st_dbe::utf8_to_unicode>)
    ( const char *src, size_t src_len, char *dst, size_t dst_max )

Converts an utf-8 string in I<src> to unicode and writes it into I<dst>.
Returns the number of bytes written, or -1 on invalid utf-8 sequence.
It is safe to make I<dst> twice bigger then I<src>.

=item int (*B<st_dbe::unicode_to_utf8>)
    ( const char *src, size_t src_len, char *dst, size_t dst_max )

Converts an unicode string in I<src> to utf-8 and writes it into I<dst>.
Returns the length of I<dst> without NULL terminating character, or -1 on
invalid unicode string.
It is safe to make I<dst> twice bigger then I<src>.

=item int (*B<st_dbe::utf8_toupper>)
    ( const char *src, size_t src_len, char *dst, size_t dst_max )

Converts an utf-8 string in I<src> to upper case and writes it into I<dst>.
Returns the length of I<dst> without \0 terminating character, or -1 on
invalid utf-8 sequence.
It is safe to make I<dst> twice bigger then I<src>.

=item int (*B<st_dbe::utf8_tolower>)
    ( const char *src, size_t src_len, char *dst, size_t dst_max )

Converts an utf-8 string in I<src> to lower case and writes it into I<dst>.
Returns the length of I<dst> without \0 terminating character, or -1 on
invalid utf-8 sequence.
It is safe to make I<dst> twice bigger then I<src>.

=back

=head3 DBE LOB functions

=over

=item SV *(*B<st_dbe::lob_stream_alloc>) ( dbe_con_t *dbe_con, void *context )

Allocates a DBE::LOB class with the drivers lob stream context.

=item void (*B<st_dbe::lob_stream_free>) ( dbe_con_t *dbe_con, void *context )

Frees a lob stream and destroys the DBE::LOB class.

=item void *(*B<st_dbe::lob_stream_get>) ( dbe_con_t *dbe_con, SV *sv )

Resturns the context of a lob stream by the given class variable.

=back

=head2 Functions provided by the Driver

Functions provided by the driver are defined in I<drv_def_t>.

=head3 The dbe_drv_t datatype

The B<drv_def_t> type defines a structure B<struct st_drv_def>. The elements of
I<st_drv_def> are explained below. Unused functions can be set to NULL.

=head3 Constants

=over

=item const int B<st_drv_def::version>

The DBE version of the driver. Must be filled with DBE_VERSION.

=item const char* B<st_drv_def::description>

A description of the driver. For example ("Dummy DBE Driver " XS_VERSION)

=back

=head3 Misc Functions

=over

=item void (*B<st_drv_def::mem_free>) ( void *ptr )

Frees memory allocated by the driver.

=back

=head3 Driver Functions

=over

=item void (*B<st_drv_def::drv_free>) ( drv_def_t *drv_def )

Can be used to cleanup the driver.

=item drv_con_t* (*B<st_drv_def::drv_connect>)
            ( dbe_con_t *dbe_con, char **args, int argc ) 

Makes a connection to the driver.

Parameter I<dbe_con> contains a pointer to the dbe connection object.
It should be hold by the driver to communicate to DBE through I<dbe_t>.

Returns a pointer to the driver connection object, or NULL on failure.

=back

=head3 Connection Methods

=over

=item int (*B<st_drv_def::con_reconnect>) ( drv_con_t *drv_con ) 

Resets the connection an restores previous settings.
Returns DBE_OK on success, or DBE_ERROR on failure.

=item void (*B<st_drv_def::con_free>) ( drv_con_t *drv_con )

Closes the connection frees its resources.

=item void (*B<st_drv_def::con_set_trace>) ( drv_con_t *drv_con, int level )

Sets the trace level to the connection object. Can be used to check the
trace level before sending the message to DBE.

=item int (*B<st_drv_def::con_set_charset>)
          ( drv_con_t *drv_con, const char *charset, size_t cs_len )

Sets the client character set.
Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_get_charset>)
          ( drv_con_t *drv_con, dbe_str_t *cs )

Gets the character set used by the client.
I<cs-E<gt>value> is allocated with 128 bytes, I<cs-E<gt>length> must not set.
Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_query>)
          ( drv_con_t *drv_con, const char *sql, size_t sql_len,
            dbe_res_t **dbe_res )

Performs a simple query.

For selectives queries the result set must be created with
C<st_dbe::alloc_result()> or C<st_dbe::vres_create()>.
(see C<the dbe_t datatype> for details)

For other type of SQL statements, UPDATE, DELETE, DROP, etc,
parameters I<dbe_res> must not set.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item u_xlong (*B<st_drv_def::con_insert_id>)
              ( drv_con_t *drv_con, const char *catalog, size_t catalog_len,
                const char *schema, size_t schema_len, const char *table,
                size_t table_len, const char *field, size_t field_len )

Returns the auto unique value generated by the previous INSERT or UPDATE
statement.

=item u_xlong (*B<st_drv_def::con_affected_rows>) ( drv_con_t *drv_con )

Returns the number of affected rows in a previous SQL operation.

Returns the number of rows changed (for UPDATE), deleted (for DELETE),
or inserted (for INSERT).

For SELECT statements, I<con_affected_rows()> works like I<res_num_rows()>.

=back

=head3 Transaction Methods

=over

=item int (*B<st_drv_def::con_auto_commit>) ( drv_con_t *drv_con, int mode )

Turns on or off auto-commit mode on queries for the database connection.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_begin_work>) ( drv_con_t *drv_con )

Turns off auto-commit mode for the database connection until transaction is
finished.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_commit>) ( drv_con_t *drv_con ) 

Commits the current transaction for the database connection.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_rollback>) ( drv_con_t *drv_con ) 

Rollbacks the current transaction for the database.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=back

=head3 Statement Methods

=over

=item int (*B<st_drv_def::con_prepare>)
          ( drv_con_t *drv_con, const char *sql, size_t sql_len,
            drv_stmt_t **drv_stmt )

Prepares a SQL statement. Parameter 'drv_stmt' must be set to a pointer
with the driver statement structure.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item void (*B<st_drv_def::stmt_free>) ( drv_stmt_t *drv_stmt ) 

Closes and frees a statement.

=item int (*B<st_drv_def::param_count>) ( drv_stmt_t *drv_stmt )

Returns the number of parameters in the statement.

=item int (*B<st_drv_def::stmt_bind_param>)
          ( drv_stmt_t *drv_stmt, u_long p_num, SV *p_val, char p_type )

Binds a value to a parameter in the statement.

Parameter I<p_num> indicates the number of the parameter starting at 1.

Parameter I<p_val> contains a Perl scalar variable.

Parameter I<p_type> can be one of the following:

=for formatter none

  'i'   corresponding value has type integer 
  'd'   corresponding value has type double 
  's'   corresponding value has type string 
  'b'   corresponding value has type binary
  '\0'  autodetect type of the value

=for formatter perl

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::stmt_param_position>)
	( drv_stmt_t *drv_stmt, const char *name, size_t name_len, u_long *p_pos )

Gets the position of parameter by given parameter name.
First parameter is starting at 1.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::stmt_execute>)
          ( drv_stmt_t *drv_stmt, dbe_res_t **dbe_res )

Executes a statement.

For selectives queries the result set must be created with
C<st_dbe::alloc_result()> or C<st_dbe::vres_create()>.
(see C<the dbe_t datatype> for details)

For other type of SQL statements, UPDATE, DELETE, DROP, etc,
parameters I<drv_res> must not set.

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=back

=head3 Result Set Methods

=over

=item void (*B<st_drv_def::res_free>) ( drv_res_t *drv_res )

Closes and frees a result set.

=item u_xlong (*B<st_drv_def::res_num_rows>) ( drv_res_t *drv_res )

Returns the number of rows in the result set.

=item int (*B<st_drv_def::res_fetch_names>)
          ( drv_res_t *drv_res, dbe_str_t *names, int *flags )

Fetches the field names representing in the result set.

The I<names> array is allocated by DBE.

Set I<flags> to DBE_FREE_ITEMS if DBE have to free the I<value> part in the
I<names> array.

Returns DBE_OK on success, or DBE_ERROR on failure.

=item int (*B<st_drv_def::res_fetch_row>) ( drv_res_t *drv_res, SV **row )

Retrieves the next row in the result set.

The I<row> array is allocated by DBE.

The values must set B<without sv_2mortal()>. Undefined values should point to
B<NULL instead of &Pl_sv_undef>.

Returns DBE_OK on success, or DBE_NORESULT if no row becomes available
anymore.

=item u_xlong (*B<st_drv_def::res_row_seek>)
              ( drv_res_t *drv_res, u_xlong offset )

Sets the actual position of the row cursor in the result set (starting at 0).
Returns the previous position of the row cursor.

=item u_xlong (*B<st_drv_def::res_row_tell>) ( drv_res_t *drv_res )

Returns the current position of the row cursor in the result set.
First row starts at 0.

=item int (*B<st_drv_def::res_fetch_field>)
          ( drv_res_t *drv_res, int offset, SV ***hash, int *flags )

Gets information about a column in the result set. Call this function
repeatedly to retrieve information about all columns in the result set.

The I<hash> array must be allocated by the driver and contains
an even number of elements in key-value style.

Set I<flags> to DBE_FREE_ARRAY to tell DBE to free the I<hash> array.

The values must set B<without sv_2mortal>. Undefined values must point to
B<NULL instead of &Pl_sv_undef>.

Returns the total number of elements, which must be an even number.

=item int (*B<st_drv_def::res_field_seek>) ( drv_res_t *drv_res, int offset )

Sets the field cursor to the given offset (starting at 0)
and returns the previous position of the field cursor.

=item int (*B<st_drv_def::res_field_tell>) ( drv_res_t *drv_res )

Returns the current position of the field cursors in the result set.

=back

=head3 Driver LOB Functions

=over

=item int (*B<st_drv_def::con_lob_size>) ( drv_con_t *drv_con, void *context )

Returns the size of the lob in bytes or characters,
or DBE_NO_DATA when it is unknown, or DBE_ERROR on error.

=item int (*B<st_drv_def::con_lob_read>)
	( drv_con_t *drv_con, void *context, int *offset, char *buffer, int length )

Reads I<length> bytes/characters from the lob starting at I<*offset>.
The new offset must be set by the driver.

Returns the number of bytes read, or DBE_NO_DATA on eof, or DBE_ERROR on error.

=item int (*B<st_drv_def::con_lob_write>)
	( drv_con_t *drv_con, void *context, int *offset, char *buffer, int length )

Write I<length> bytes/characters to the lob starting at I<*offset>.
The new offset must be set by the driver.

Returns the number of bytes written, or DBE_ERROR on error.

=item int (*B<st_drv_def::con_lob_close>)
	( drv_con_t *drv_con, void *context )

Closes the lob stream.

Returns DBE_OK on success, or DBE_ERROR on failure.

=back

=head3 Misc Functions

=over

=item SV* (*B<st_drv_def::drv_quote>)
          ( drv_con_t *drv_con, const char *str, size_t str_len )

Quote a value for use as a literal value in an SQL statement, by escaping any
special characters (such as quotation marks) contained within the string and
adding the required type of outer quotation marks.

Returns a Perl SV B<without> setting B<sv_2mortal>, or NULL on undefined value.

=item SV* (*B<st_drv_def::drv_quote_bin>)
          ( drv_con_t *drv_con, const char *bin, size_t bin_len )

Quote a value for use as a binary value in an SQL statement. Skip this function
if the driver does not support binary values in SQL statements.

Returns a Perl SV B<without> setting B<sv_2mortal>, or NULL on undefined value.

=item SV* (*B<st_drv_def::drv_quote_id>)
          ( drv_con_t *drv_con, const char **args, int argc )

Quote identifiers (table name etc.) for use in an SQL statement, by escaping
any special characters it contains and adding the required type of outer
quotation marks.

Returns a Perl SV B<without> setting B<sv_2mortal>, or NULL on undefined value.

=back

=head3 Information and Catalog Functions

=over

=item int (*B<con_getinfo>)
          ( drv_con_t *drv_con, int type, void *val, int cbvalmax, int *pcbval,
            enum dbe_getinfo_type *rtype )

Get general information about the DBMS that the application is currently
connected to.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<type>

Type of information desired. (should be compatible to ODBC 3.0)

=item I<val>

[output] Pointer to buffer where this function stores the desired information.
It has a size of 256 bytes minimum.
Depending on the type of information being retrieved, 4 types of
information can be returned: 

=for formatter none

  - DBE_GETINFO_SHORTINT   / 16-bit integer value 
  - DBE_GETINFO_INTEGER    / 32-bit integer value 
  - DBE_GETINFO_32BITMASK  / 32-bit binary value 
  - DBE_GETINFO_STRING     / Null-terminated character string

=for formatter perl

the returned type must be set to I<rtype>

=item I<cbvalmax>

Maximum length of the buffer pointed by I<val> pointer.

=item I<pcbval>

[output] Pointer to location where this function returns the total number of
bytes available to return the desired information. 

=item I<rtype>

[output] The return type of information. See also I<val>

=back

B<Return Codes>

=over

=item * DBE_OK

=item * DBE_ERROR

=item * DBE_CONLOST

=item * DBE_GETINFO_ERR_INVALIDARG

=item * DBE_GETINFO_ERR_TRUNCATE

=back

=item int (*B<st_drv_def::con_type_info>)
          ( drv_con_t *drv_con, int sql_type, dbe_res_t **dbe_res )

Returns information about the data types that are supported by the DBMS.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<sql_type>

The SQL data type being queried. The supported types are: 

=over

=item * SQL_ALL_TYPES

=item * SQL_BIGINT

=item * SQL_BINARY

=item * SQL_VARBINARY

=item * SQL_CHAR

=item * SQL_DATE

=item * SQL_DECIMAL

=item * SQL_DOUBLE

=item * SQL_FLOAT

=item * SQL_INTEGER

=item * SQL_NUMERIC

=item * SQL_REAL

=item * SQL_SMALLINT

=item * SQL_TIME

=item * SQL_TIMESTAMP

=item * SQL_VARCHAR

=back

If SQL_ALL_TYPES is specified, information about all supported data types
would be returned in ascending order by TYPE_NAME. All unsupported data
types would be absent from the result set.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/type_info> for a description
of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_data_sources>)
          ( drv_con_t *drv_con, char *wild, size_t wild_len,
            SV ***names, int *count )

Retrieves a list of data sources (databases) available.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<wild>

I<wild> may contain the wildcard characters "%" or "_", or may be a NULL
pointer to match all datasources

=item I<wild_len>

The length of I<wild> in characters.

=item I<names>

[output] I<names> contains the list of data sources. The array must be
allocated by the driver.

=item I<count>

[output] Number of datasources in I<names>

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_tables>)
          ( drv_con_t *drv_con, char *catalog, size_t catalog_len,
            char *schema, size_t schema_len, char *table, size_t table_len,
            char *type, size_t type_len, dbe_res_t **dbe_res )

Gets a result set that can be used to fetch information about tables
and views that exist in the database.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<catalog>

Buffer that may contain a pattern-value to qualify the result set.
Catalog is the first part of a three-part table name. 

=item I<catalog_len>

Length of I<catalog>.

=item I<schema>

Buffer that may contain a pattern-value to qualify the result set by
schema name.

=item I<schema_len>

Length of I<schema>.

=item I<table>

Buffer that may contain a pattern-value to qualify the result set by
table name.

=item I<table_len>

Length of I<table>.

=item I<type>

Buffer that may contain a value list to qualify the result set by table type.

The value list is a list of values separated by commas for the types of
interest. Valid table type identifiers may include: ALL, BASE TABLE, TABLE,
VIEW, SYSTEM TABLE. If I<type> argument is a NULL pointer or a zero length
string, then this is equivalent to specifying all of the possibilities for
the table type identifier.

If SYSTEM TABLE is specified, then both system tables and system
views (if there are any) are returned.

=item I<type_len>

Length of I<type>.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/tables> for a description
of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_columns>)
          ( drv_con_t *drv_con, char *catalog, size_t catalog_len,
            char *schema, size_t schema_len, char *table, size_t table_len,
            char *column, size_t column_len, dbe_res_t **dbe_res )  

Gets a list of columns in the specified tables.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<catalog>

Buffer that may contain a pattern-value to qualify the result set.
Catalog is the first part of a three-part table name. 

=item I<catalog_len>

Length of I<catalog>.

=item I<schema>

Buffer that may contain a pattern-value to qualify the result set by
schema name.

=item I<schema_len>

Length of I<schema>.

=item I<table>

Buffer that may contain a pattern-value to qualify the result set by
table name.

=item I<table_len>

Length of I<table>.

=item I<column>

Buffer that may contain a pattern-value to qualify the result set by
column name.

=item I<column_len>

Length of I<column>.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/columns> for a description
of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_primary_keys>)
          ( drv_con_t *drv_con, char *catalog, size_t catalog_len,
            char *schema, size_t schema_len, char *table, size_t table_len,
            dbe_res_t **dbe_res ) 

Gets a list of column names that comprise the primary key for a table.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<catalog>

Catalog qualifier of a 3 part table name.

=item I<catalog_len>

Length of I<catalog>.

=item I<schema>

Schema qualifier of table name.

=item I<schema_len>

Length of I<schema>.

=item I<table>

Table name.

=item I<table_len>

Length of I<table>.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/primary_keys> for a
description of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_foreign_keys>)
          ( drv_con_t *drv_con, char *pk_cat, size_t pk_cat_len,
            char *pk_schem, size_t pk_schem_len, char *pk_table,
            size_t pk_table_len, char *fk_cat, size_t fk_cat_len,
            char *fk_schem, size_t fk_schem_len, char *fk_table,
            size_t fk_table_len, dbe_res_t **dbe_res )

Gets information about foreign keys for the specified table.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<pk_cat>

Catalog qualifier of the primary key table.

=item I<pk_cat_len>

Length of I<pk_cat>.

=item I<pk_schem>

Schema qualifier of the primary key table.

=item I<pk_schem_len>

Length of I<pk_schem>.

=item I<pk_table>

Name of the table name containing the primary key.

=item I<pk_table_len>

Length of I<pk_table>.

=item I<fk_cat>

Catalog qualifier of the table containing the foreign key.

=item I<fk_cat_len>

Length of I<fk_cat>.

=item I<fk_schem>

Schema qualifier of the table containing the foreign key.

=item I<fk_schem_len>

Length of I<fk_schem>.

=item I<fk_table>

Name of the table containing the foreign key.

=item I<fk_table_len>

Length of I<fk_table>.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/foreign_keys> for a
description of the columns.

=back

B<Usage>

If I<pk_table> contains a table name, and I<fk_table> is an empty string,
function returns a result set that contains the primary key of the specified
table and all of the foreign keys (in other tables) that refer to it.

If I<fk_table> contains a table name, and I<pk_table> is an empty string,
function returns a result set that contains all of the foreign keys in the
specified table and the primary keys (in other tables) to which they refer.

If both I<pk_table> and I<fk_table> contain table names, function returns
the foreign keys in the table specified in FKTableName that refer to the
primary key of the table specified in PKTableName. This should be one
key at the most.

If the schema qualifier argument that is associated with a table name is
not specified, then the schema name defaults to the one currently in
effect for the current connection.

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.


=item int (*B<st_drv_def::con_statistics>)
          ( drv_con_t *drv_con, const char *catalog, size_t catalog_len,
            const char *schema, size_t schema_len, const char *table,
            size_t table_len, int unique_only, dbe_res_t **dbe_res )

Retrieves a list of statistics about a single table and the indexes associated
with the table.

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<catalog>

Catalog qualifier of a 3 part table name.

=item I<catalog_len>

Length of I<catalog>.

=item I<schema>

Schema qualifier of table name.

=item I<schema_len>

Length of I<schema>.

=item I<table>

Table name.

=item I<table_len>

Length of I<table>.

=item I<unique_only>

On a TRUE value only unique indexes are returned, else all indexes are
returned.

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/statistics> for a description
of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=item int (*B<st_drv_def::con_special_columns>)
          ( drv_con_t *con, int identifier_type, const char *catalog,
            size_t catalog_len, const char *schema, size_t schema_len,
            const char *table, size_t table_len, int scope, int nullable,
            dbe_res_t **dbe_res )

Retrieves the following information about columns within a specified table:

=over

=item * The optimal set of columns that uniquely identifies a row in the table.

=item * Columns that are automatically updated when any value in the row is
updated by a transaction.

=back

B<Parameters>

=over

=item I<drv_con>

Pointer to driver connection structure.

=item I<identifier_type>

Must be one of the following values:

SQL_BEST_ROWID: Returns the optimal column or set of columns that, by
retrieving values from the column or columns, allows any row in the specified
table to be uniquely identified. A column can be either a pseudo-column
specifically designed for this purpose (as in Oracle ROWID or Ingres TID) or
the column or columns of any unique index for the table. 

SQL_ROWVER: Returns the column or columns in the specified table, if any,
that are automatically updated by the data source when any value in the
row is updated by any transaction (as in SQLBase ROWID or Sybase TIMESTAMP).

=item I<catalog>

Catalog qualifier of a 3 part table name.

=item I<catalog_len>

Length of I<catalog>.

=item I<schema>

Schema qualifier of table name.

=item I<schema_len>

Length of I<schema>.

=item I<table>

Table name.

=item I<table_len>

Length of I<table>.

=item I<scope>

Minimum required scope of the rowid. The returned rowid may be of greater
scope. Must be one of the following:

SQL_SCOPE_CURROW: The rowid is guaranteed to be valid only while positioned on
that row. A later reselect using rowid may not return a row if the row was
updated or deleted by another transaction. 

SQL_SCOPE_TRANSACTION: The rowid is guaranteed to be valid for the duration
of the current transaction. 

SQL_SCOPE_SESSION: The rowid is guaranteed to be valid for the duration of
the session (across transaction boundaries). 

=item I<nullable>

Determines whether to return special columns that can have a NULL value.
Must be one of the following:

SQL_NO_NULLS: Exclude special columns that can have NULL values. Some drivers
cannot support SQL_NO_NULLS, and these drivers will return an empty result
set if SQL_NO_NULLS was specified. Applications should be prepared for
this case and request SQL_NO_NULLS only if it is absolutely required. 

SQL_NULLABLE: Return special columns even if they can have NULL values. 

=item I<dbe_res>

[output] A pointer to a result set, created with I<st_dbe::alloc_result> or
I<st_dbe::vres_create>. See L<the DBE manpage|DBE/special_columns> for a
description of the columns.

=back

B<Return Values>

Returns DBE_OK on success, or DBE_CONLOST on connection error, or DBE_ERROR
on other failure.

=back

=head1 AUTHORS

Navalla org., Christian Mueller, L<http://www.navalla.org/>

=head1 COPYRIGHT

DBE::Driver is a part of the DBE module and may be used under the
same license agreement.

=head1 SEE ALSO

Project Homepage L<http://www.navalla.org/dbe/>

=cut
