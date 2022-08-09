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

if [[ ! -d images && ! -h images ]]; then
    echo 'The images directory/symlink does not exist. Unpack it from the tar file at'
    echo 'https://qoiformat.org/benchmark/'
    exit 1
fi

save=$(git branch --show-current)
if [ -z "$save" ]; then
    save=$(git log -1 --pretty=format:"%H")
fi

echo "Commit   CmpRatio   -------------gcc------------  ------------clang-----------"

i=0
while [ $i -lt ${N:-9999} ]; do
  set +e
  this_hash=$(git log -1 --pretty=format:"%h")
  this_subj=$(git log -1 --pretty=format:"%s")
  if [ $this_hash = 2feeb80 ]; then
    break
  fi
  this_gcc=$(CC=gcc   ./run_benchmarks.sh images | grep QOI | head -n 1 | \
      sed -e 's,QOI.   ,,' -e 's,CmpRatio  ,CR,' \
      -e 's,EncMPixels/s  ,gccEnc,' -e 's,DecMPixels/s.*,gccDec,')
  this_cla=$(CC=clang ./run_benchmarks.sh images | grep QOI | head -n 1 | \
      sed -e 's,QOI.   ,,' -e 's,.*CmpRatio   ,,' \
      -e 's,EncMPixels/s  ,claEnc,' -e 's,DecMPixels/s.*,claDec,')
  echo "$this_hash $this_gcc $this_cla  $this_subj"
  git checkout --quiet HEAD^
  i=$((i + 1))
done

git checkout --quiet $save
