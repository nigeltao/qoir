#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}
LDFLAGS=${LDFLAGS:-}

mkdir -p out
echo 'Compiling out/round_trip_tests'
$CC $CFLAGS test/round_trip_tests.c $LDFLAGS -o out/round_trip_tests
echo 'Running   out/round_trip_tests'
out/round_trip_tests ${@:-test/data}
