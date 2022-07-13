// Copyright 2022 Nigel Tao.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define QOIR_IMPLEMENTATION
#include "../src/qoir.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "../third_party/stb/stb_image.h"

// Utilities should be #include'd after qoir.h
#include "../util/check_round_trip.c"

// ----

int  //
do_test_round_trip(const char* testname, const char* filename) {
  FILE* f = fopen(filename, "r");
  if (!f) {
    printf("%s: %s: %s\n", testname, filename, strerror(errno));
    return 1;
  }
  const char* result = check_round_trip(f);
  fclose(f);
  if (result) {
    printf("%s: %s: %s\n", testname, filename, result);
    return 1;
  }
  return 0;
}

int  //
test_round_trip(void) {
  if (do_test_round_trip(__func__, "test/data/bricks-color.png") ||
      do_test_round_trip(__func__, "test/data/harvesters.png") ||
      do_test_round_trip(__func__, "test/data/hibiscus.primitive.png") ||
      do_test_round_trip(__func__, "test/data/hibiscus.regular.png")) {
    return 1;
  }
  printf("%s: OK\n", __func__);
  return 0;
}

// ----

int  //
main(int argc, char** argv) {
  return test_round_trip();
}
