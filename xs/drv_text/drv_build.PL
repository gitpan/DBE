#!perl

open $fh, "< drv_build.dat";
chomp( $build = <$fh> );
close $fh;

$build ++;

open $fh, "> drv_build.dat";
print $fh $build;
close $fh;

open $fh, "> drv_build.h";
print $fh <<EOT;
#undef DRV_BUILD
#define DRV_BUILD $build

#undef DRV_BUILD_STRING
#define DRV_BUILD_STRING "$build"
EOT
close $fh;
