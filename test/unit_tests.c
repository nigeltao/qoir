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
do_test_swizzle(const char* testname,
                const char* funcname,
                qoir_private_swizzle_func swizzle_func,
                const uint8_t* want) {
  static const uint8_t src[32] = {
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  //
      0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,  //
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  //
      0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,  //
  };

  uint8_t have[32];
  for (int i = 0; i < 32; i++) {
    have[i] = 0x77;
  }
  (*swizzle_func)(have, 0, src, 0, 8, 1);

  if (memcmp(have, want, 32)) {
    printf("%s: %s:\nhave:\n   ", testname, funcname);
    for (int i = 0; i < 32; i++) {
      if (i == 16) {
        printf("\n   ");
      }
      printf(" %02X", have[i]);
    }
    printf("\nwant:\n   ");
    for (int i = 0; i < 32; i++) {
      if (i == 16) {
        printf("\n   ");
      }
      printf(" %02X", want[i]);
    }
    printf("\n");
    return 1;
  }
  return 0;
}

int  //
test_swizzle(void) {
  static const uint8_t want__copy_4[32] = {
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  //
      0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,  //
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  //
      0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,  //
  };

  static const uint8_t want__bgr__bgra[32] = {
      0x50, 0x51, 0x52, 0x54, 0x55, 0x56, 0x58, 0x59,  //
      0x5A, 0x5C, 0x5D, 0x5E, 0x60, 0x61, 0x62, 0x64,  //
      0x65, 0x66, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E,  //
      0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,  //
  };

  static const uint8_t want__bgr__rgba[32] = {
      0x52, 0x51, 0x50, 0x56, 0x55, 0x54, 0x5A, 0x59,  //
      0x58, 0x5E, 0x5D, 0x5C, 0x62, 0x61, 0x60, 0x66,  //
      0x65, 0x64, 0x6A, 0x69, 0x68, 0x6E, 0x6D, 0x6C,  //
      0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,  //
  };

  static const uint8_t want__bgra__bgr[32] = {
      0x50, 0x51, 0x52, 0xFF, 0x53, 0x54, 0x55, 0xFF,  //
      0x56, 0x57, 0x58, 0xFF, 0x59, 0x5A, 0x5B, 0xFF,  //
      0x5C, 0x5D, 0x5E, 0xFF, 0x5F, 0x60, 0x61, 0xFF,  //
      0x62, 0x63, 0x64, 0xFF, 0x65, 0x66, 0x67, 0xFF,  //
  };

  static const uint8_t want__bgra__rgb[32] = {
      0x52, 0x51, 0x50, 0xFF, 0x55, 0x54, 0x53, 0xFF,  //
      0x58, 0x57, 0x56, 0xFF, 0x5B, 0x5A, 0x59, 0xFF,  //
      0x5E, 0x5D, 0x5C, 0xFF, 0x61, 0x60, 0x5F, 0xFF,  //
      0x64, 0x63, 0x62, 0xFF, 0x67, 0x66, 0x65, 0xFF,  //
  };

  static const uint8_t want__bgra__rgba[32] = {
      0x52, 0x51, 0x50, 0x53, 0x56, 0x55, 0x54, 0x57,  //
      0x5A, 0x59, 0x58, 0x5B, 0x5E, 0x5D, 0x5C, 0x5F,  //
      0x62, 0x61, 0x60, 0x63, 0x66, 0x65, 0x64, 0x67,  //
      0x6A, 0x69, 0x68, 0x6B, 0x6E, 0x6D, 0x6C, 0x6F,  //
  };

  if (do_test_swizzle(__func__, "qoir_private_swizzle__copy_4",
                      qoir_private_swizzle__copy_4, want__copy_4) ||
      do_test_swizzle(__func__, "qoir_private_swizzle__bgr__bgra",
                      qoir_private_swizzle__bgr__bgra, want__bgr__bgra) ||
      do_test_swizzle(__func__, "qoir_private_swizzle__bgr__rgba",
                      qoir_private_swizzle__bgr__rgba, want__bgr__rgba) ||
      do_test_swizzle(__func__, "qoir_private_swizzle__bgra__bgr",
                      qoir_private_swizzle__bgra__bgr, want__bgra__bgr) ||
      do_test_swizzle(__func__, "qoir_private_swizzle__bgra__rgb",
                      qoir_private_swizzle__bgra__rgb, want__bgra__rgb) ||
      do_test_swizzle(__func__, "qoir_private_swizzle__bgra__rgba",
                      qoir_private_swizzle__bgra__rgba, want__bgra__rgba)) {
    return 1;
  }
  printf("%s: OK\n", __func__);
  return 0;
}

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
  return test_swizzle() ||  //
         test_round_trip();
}
