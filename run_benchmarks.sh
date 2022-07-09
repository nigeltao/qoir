#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}

mkdir -p gen
echo Compiling...
$CC $CFLAGS test/benchmarks.c -o gen/benchmarks
echo Running...
# The "| awk etc" sorts by the final column.
gen/benchmarks ${@:--v test/data} | awk '{print $NF,$0}' | sort | cut -f2- -d' '
