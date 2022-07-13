#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall}

mkdir -p out
echo Compiling...
$CC $CFLAGS test/unit_tests.c -o out/unit_tests
echo Running...
out/unit_tests
