#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}
LDFLAGS=${LDFLAGS:-}

mkdir -p out
echo 'Compiling out/benchmarks'
$CC $CFLAGS test/benchmarks.c $LDFLAGS -o out/benchmarks
echo 'Running   out/benchmarks'
# The "| awk etc" sorts by the final column.
out/benchmarks ${@:--v test/data} | awk '{print $NF,$0}' | sort | cut -f2- -d' '
