#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall}

mkdir -p gen
echo Compiling...
$CC $CFLAGS test/unit_tests.c -o gen/unit_tests
echo Running...
gen/unit_tests
