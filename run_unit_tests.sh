#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall}
LDFLAGS=${LDFLAGS:-}

mkdir -p out
echo 'Compiling out/unit_tests'
$CC $CFLAGS test/unit_tests.c $LDFLAGS -o out/unit_tests
echo 'Running   out/unit_tests'
out/unit_tests
