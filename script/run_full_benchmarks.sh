#!/bin/bash -eu
# Copyright 2022 Nigel Tao.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# ----------------

CC=${CC:-gcc}
CXX=${CC:-g++}
CFLAGS=${CFLAGS:--Wall -O3}
CXXFLAGS=${CFLAGS:-Wall -O3}
LDFLAGS=${LDFLAGS:-}

mkdir -p out

# The JXL-QOIR adapter assumes that you've built libjxl (per its README.md) as
# ../libjxl/build relative to this directory.
#
# FASTLL_ENABLE_AVX2_INTRINSICS (which obviously assumes an AVX2 capable CPU)
# configures libjxl's experimental/fast_lossless encoder.
echo 'Compiling out/jxl_adapter.o'
$CXX -c -march=native \
    -DFASTLL_ENABLE_AVX2_INTRINSICS=1 \
    -I../libjxl/build/lib/include \
    -I../libjxl/lib/include \
    $CXXFLAGS -Wno-unknown-pragmas adapter/jxl_adapter.cpp \
    $LDFLAGS -o out/jxl_adapter.o

echo 'Compiling out/png_fpng_adapter.o'
$CXX -c -march=native \
    $CXXFLAGS adapter/png_fpng_adapter.cpp \
    $LDFLAGS -o out/png_fpng_adapter.o

echo 'Compiling out/png_fpnge_adapter.o'
$CXX -c -march=native \
    $CXXFLAGS adapter/png_fpnge_adapter.cpp \
    $LDFLAGS -o out/png_fpnge_adapter.o

echo 'Compiling out/png_libpng_adapter.o'
$CC  -c -march=native \
    $CXXFLAGS adapter/png_libpng_adapter.c \
    $LDFLAGS -o out/png_libpng_adapter.o

echo 'Compiling out/png_stb_adapter.o'
$CC  -c -march=native \
    $CXXFLAGS adapter/png_stb_adapter.c \
    $LDFLAGS -o out/png_stb_adapter.o

echo 'Compiling out/png_wuffs_adapter.o'
$CC  -c -march=native \
    $CXXFLAGS adapter/png_wuffs_adapter.c \
    $LDFLAGS -o out/png_wuffs_adapter.o

echo 'Compiling out/qoi_adapter.o'
$CC  -c -march=native \
    $CXXFLAGS adapter/qoi_adapter.c \
    $LDFLAGS -o out/qoi_adapter.o

echo 'Compiling out/webp_adapter.o'
$CC  -c -march=native \
    $CXXFLAGS adapter/webp_adapter.c \
    $LDFLAGS -o out/webp_adapter.o

echo 'Compiling out/full_benchmarks'
$CC -DCONFIG_FULL_BENCHMARKS=1 \
    $CFLAGS test/benchmarks.c \
    out/jxl_adapter.o \
    out/png_fpng_adapter.o \
    out/png_fpnge_adapter.o \
    out/png_libpng_adapter.o \
    out/png_stb_adapter.o \
    out/png_wuffs_adapter.o \
    out/qoi_adapter.o \
    out/webp_adapter.o \
    -L../libjxl/build \
    $LDFLAGS -ljxl -lpng -lstdc++ -lwebp -o out/full_benchmarks

echo 'Running   out/full_benchmarks'
# The "| awk etc" sorts by the final column.
LD_LIBRARY_PATH=../libjxl/build out/full_benchmarks ${@:--v test/data} \
    | awk '{print $NF,$0}' | sort | cut -f2- -d' '
