#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}

mkdir -p out
echo Compiling out/qoirconv
$CC $CFLAGS cmd/qoirconv.c -o out/qoirconv
