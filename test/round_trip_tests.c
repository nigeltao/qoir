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
#include <sys/time.h>

#define QOIR_IMPLEMENTATION
#include "../src/qoir.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "../third_party/stb/stb_image.h"

// Utilities should be #include'd after qoir.h
#include "../util/check_round_trip.c"
#include "../util/walk_directory.c"

// ----

const char*  //
my_enter_callback(void* context, uint32_t depth, const char* dirname) {
  return NULL;
}

const char*  //
my_exit_callback(void* context, uint32_t depth, const char* dirname) {
  return NULL;
}

const char*  //
my_file_callback(void* context,
                 uint32_t depth,
                 const char* dirname,
                 const char* filename) {
  const char* testname = (const char*)context;
  size_t n = strlen(filename);
  if ((n < 4) || strcmp(filename + n - 4, ".png")) {
    return NULL;
  }

  FILE* f = fopen(filename, "r");
  if (!f) {
    printf("%s%s%s: could not open file: %s\n", testname, dirname, filename,
           strerror(errno));
    return "fopen failed";
  }
  const char* result = check_round_trip(f);
  fclose(f);
  printf("%s%s%s: %s\n", testname, dirname, filename, result ? result : "OK");
  return result;
}

int  //
check(char* arg) {
  const char* status_message = NULL;

  DIR* d = opendir(arg);
  if (d) {
    status_message = walk_directory(d, arg, &my_enter_callback,
                                    &my_exit_callback, &my_file_callback);
    closedir(d);
  } else {
    struct stat s;
    if (stat(arg, &s)) {
      status_message = strerror(errno);
    } else {
      status_message = my_file_callback("", 0, "", arg);
    }
  }

  if (status_message) {
    printf("could not walk \"%s\": %s\n", arg, status_message);
    return 1;
  }
  return 0;
}

// ----

int  //
main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    int result = check(argv[i]);
    if (result) {
      return result;
    }
  }

  return 0;
}
