package DBE::Driver::TEXT;
# =============================================================================
# DBE Text Driver
# =============================================================================

# enable for debugging
#use strict;
#use warnings;

our $VERSION;

BEGIN {
	$VERSION = '0.99.01';
	require XSLoader;
	XSLoader::load( __PACKAGE__, $VERSION );
}

1;


__END__

=head1 NAME

DBE::Driver::TEXT - Text Driver for the Perl Database Express Engine

=head1 SYNOPSIS

  use DBE;
  
  $con = DBE->connect( 'Provider=Text;DBQ=/path/to/text-files' );
  
  $res = $con->query( 'SELECT * FROM "table.csv"' );
      or
  $stmt = $con->prepare( 'SELECT * FROM "table.csv"' );
  $res = $stmt->execute();
      or
  $con->prepare( 'SELECT * FROM "table.csv"' );
  $res = $con->execute();

=head1 EXAMPLE

NOTE: This requires a schema.ini file in the same path as the delimited text
file. You must change the columns in the schema.ini file to correspond to
your delimited file. Also, you must change the path in the connect string to
the correct path for your file.

To create a schema.ini file, use a text editor. Below are the schema.ini
entries for the sample1.txt file listed below in this example: 

B<schema.ini>

=for formatter none

  [sample1.txt]
  ColNameHeader = False
  Format = CSVDelimited
  CharacterSet = ANSI
  Col1 = ProductID int
  Col2 = ProductName char width 30
  Col3 = QuantityPerUnit char width 30
  Col4 = UnitPrice double
  Col5 = Discontinued int  

=for formatter perl

You could copy and paste the following delimited file example into a text editor
and save it as sample1.txt. You could then modify the DBQ path, below, to point
to the comma delimited file sample1.txt. 

B<sample1.txt>

=for formatter none

  1,Chai,10 boxes x 20 bags,18.00,0
  2,Chang,24 - 12 oz bottles,19.00,0
  3,Aniseed Syrup,12 - 550 ml bottles,10.00,0
  4,Chef Anton's Cajun Seasoning,48 - 6 oz jars,22.00,0
  5,Chef Anton's Gumbo Mix,36 boxes,21.35,1
  6,Grandma's Boysenberry Spread,12 - 8 oz jars,25.00,0
  7,Uncle Bob's Organic Dried Pears,12 - 1 lb pkgs.,30.00,0 

=for formatter perl

NOTE: Make sure when you paste it in the text editor that you do not leave
any blank lines at the top of the file. 

B<sample1.pl>

  use DBE;
  
  $con = DBE->connect(
      'Provider' => 'Text',
      'DBQ' => 'E:\Samples\Text\CommaDelimited',
      'Charset' => 'UTF-8',
  );
  
  $res = $con->query( 'SELECT * FROM "sample1.txt"' );

=head1 DESCRIPTION

DBE::Driver::Text is a DBE driver for text files. It is almost
compatible to the Microsoft Text Driver.

=head2 CONNECT

  use DBE;
  
  DBE->connect( 'Provider=Text;DBQ=.' );

The parameters are specified as follows:

=over

=item * B<PROVIDER> [Text]

Provider must be "Text".

=item * B<DBQ> | B<DEFAULTDIR> [path]

Path to text files. Defaults to the current directory.

=item * B<USESCHEMA>  [1|0|Yes|No|True|False]

A boolean value to enable oder disable "schema.ini". Defaults to Yes.

=item * B<COLNAMEHEADER>  [1|0|Yes|No|True|False]

A boolean value to enable or disable column names in first row of the text
files. Defaults to Yes.

=item * B<QUOTECHAR> [char]

The character for quoting values. Defaults to '"'.

=item * B<DEC> | B<DECIMALSYMBOL> [char]

The character for the decimal point. Defaults to '.'.

=item * B<FORMAT> [CSVDelimited|TABDelimited|Delimited(;)]

Format definition of Text files. Format I<CSVDelimited> delimits columns
by comma, I<TABDelimited> delimits columns by tab and I<Delimited(;)>
defines a custom delimiter, where ';' can be replaced with any character
except of '"'.

=item * B<DELIMITER> [char]

Column delimiter character. Defaults to ';'. Overwrites FORMAT.

=item * B<MSR> | B<MAXSCANROWS> [num]

Maximum number of rows to scan for to detect data types of a table.
Defaults to 25.

=item * B<CHARSET> | B<CHARACTERSET> [string]

Client character set. Supported charsets are ANSI and UTF-8. Default is ANSI.

=item * B<EXTENSIONS> [string]

Lists of file name extensions for the Text files on the data source separated
by comma. Default is "csv,txt"

=back

=head1 SQL SYNTAX

=head2 SELECT

=for formatter sql

  SELECT
      select_expr, ...
    FROM table_references
    WHERE where_condition
    [GROUP BY {col_name | expr}, ...]
    [ORDER BY {col_name | expr} [ASC | DESC], ...]
    [LIMIT {[offset,] row_count | row_count OFFSET offset}]
  
  table_references:
    table_reference [, table_reference] ...
  
  table_reference:
    tbl_name [[AS] alias]

=for formatter perl

=head2 CREATE TABLE

=for formatter sql

  CREATE TABLE [IF NOT EXISTS] tbl_name
    (create_definition, ...)
  
  create_definition:
    col_name column_definition
  
  column_definition:
    data_type [NOT NULL | NULL] [DEFAULT default_value]
  
  data_type:
      SMALLINT[(length)]
    | INT[(length)]
    | INTEGER[(length)]
    | REAL[(length)]
    | DOUBLE[(length)]
    | FLOAT[(length)]
    | CHAR(length)
    | VARCHAR(length)
    | BINARY(length)
    | VARBINARY(length)
    | BLOB

=for formatter perl

=head2 DROP TABLE

=for formatter sql

  DROP TABLE [IF EXISTS] tbl_name

=for formatter perl

=head2 INSERT

=for formatter sql

  INSERT INTO tbl_name [(col_name, ...)]
    VALUES (expr, ...)

=for formatter perl

=head2 UPDATE

  UPDATE tbl_name
    SET col_name1=expr1 [, col_name2=expr2 ...]
    [WHERE where_condition]

B<Note>: UPDATE creates a temporary table and fills it with the updated
data. After then the temporary table will be replaced with the old table.

=head2 DELETE

=for formatter sql

  DELETE FROM tbl_name
    [WHERE where_condition]

=for formatter perl

B<Note>: DELETE creates a temporary table and fills it with the undeleted
data. After then the temporary table will be replaced with the old table.

=head1 SUPPORTED FUNCTIONS

=head2 Aggregate functions

=over

=item B<COUNT> (expr)

=item B<SUM> (expr)

=item B<AVG> (expr)

=item B<MIN> (expr)

=item B<MAX> (expr)

=back

=head2 Numeric functions

=over

=item B<ABS> (expr)

=item B<ROUND> (expr [, prec])

=back

=head2 String functions

=over

=item B<CONCAT> (expr1, expr2, ...)

=item B<TRIM> ([{LEADING|TRAILING|BOTH}] [expr FROM]  expr)

=item B<LTRIM> (expr)

=item B<RTRIM> (expr)

=item B<LOWER> (expr) | B<LCASE> (expr)

=item B<UPPER> (expr) | B<UCASE> (expr)

=item B<LENGTH> (expr) | B<OCTET_LENGTH> (expr)

=item B<CHAR_LENGTH> (expr) | B<CHARACTER_LENGTH> (expr)

=item B<LOCATE> (substr, str [, start])

=item B<POSITION> (substr IN str)

=item B<SUBSTRING> | B<SUBSTR> (expr {,|FROM} index [{,|FOR} count])

=back

=head2 Date and time functions

=over

=item B<CURRENT_TIMESTAMP> | B<CURRENT_TIMESTAMP> () | B<NOW> ()

=item B<CURRENT_DATE> | B<CURRENT_DATE> ()

=item B<CURRENT_TIME> | B<CURRENT_TIME> ()

=back

=head2 Other functions

=over

=item B<CONVERT> (expr, type)

=item B<CONVERT> (expr USING charset)

=item B<IF> (expr_eval, expr_true, expr_false)

=back

=head1 LIMITATIONS

Table format FIXEDLENGTH is not supported.

=head1 AUTHORS

Written by Christian Mueller

=head1 COPYRIGHT

The DBE::Driver::TEXT module is free software. You may distribute under the
terms of either the GNU General Public License or the Artistic License, as
specified in the Perl README file.

=cut
