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
CFLAGS=${CFLAGS:--Wall -O3}
LDFLAGS=${LDFLAGS:-}

mkdir -p out
echo 'Compiling out/full_benchmarks'
$CC $CFLAGS -DCONFIG_FULL_BENCHMARKS=1 test/benchmarks.c $LDFLAGS -o out/full_benchmarks
echo 'Running   out/full_benchmarks'
# The "| awk etc" sorts by the final column.
out/full_benchmarks ${@:--v test/data} | awk '{print $NF,$0}' | sort | cut -f2- -d' '
