#!/bin/bash -eu

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3}

mkdir -p out
echo 'Compiling out/qoirview'
$CC $CFLAGS cmd/qoirview.c -lSDL2 -o out/qoirview
