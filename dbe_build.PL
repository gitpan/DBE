#!perl

open $fh, "< dbe_build.dat";
chomp( $build = <$fh> );
close $fh;

$build ++;

open $fh, "> dbe_build.dat";
print $fh $build;
close $fh;

open $fh, "> dbe_build.h";
print $fh <<EOT;
#undef DBE_BUILD
#define DBE_BUILD $build

#undef DBE_BUILD_STRING
#define DBE_BUILD_STRING "$build"
EOT
close $fh;
