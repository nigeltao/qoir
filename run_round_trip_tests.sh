#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}

mkdir -p out
echo Compiling...
$CC $CFLAGS test/round_trip_tests.c -o out/round_trip_tests
echo Running...
out/round_trip_tests ${@:-test/data}
