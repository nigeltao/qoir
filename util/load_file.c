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

#ifndef INCLUDE_GUARD_UTIL_LOAD_FILE
#define INCLUDE_GUARD_UTIL_LOAD_FILE

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct load_file_result_struct {
  const char* status_message;
  void* owned_memory;
  uint8_t* dst_ptr;
  size_t dst_len;
  bool truncated;
} load_file_result;

static load_file_result  //
load_file(               //
    FILE* f,             //
    uint64_t max_incl_len) {
  load_file_result result = {0};
  if (!f) {
    result.status_message = "#load_file: NULL file";
    return result;
  } else if (ferror(f)) {
    result.status_message = "#load_file: invalid file";
    return result;
  }

  uint8_t* ptr = NULL;
  size_t len = 0;
  size_t cap = 0;
  bool truncated = false;

  for (int num_eintr = 0;;) {
    if (len >= max_incl_len) {
      truncated = true;
      break;
    } else if (len >= cap) {
      if ((max_incl_len < 16777216) || (cap > (max_incl_len - 16777216))) {
        cap = max_incl_len;
      } else if (cap == 0) {
        cap = 65536;
      } else if (cap < 16777216) {
        cap *= 2;
      } else if (cap <= (SIZE_MAX - 16777216)) {
        cap += 16777216;
      } else {
        free(ptr);
        result.status_message = "#load_file: out of memory";
        return result;
      }
      uint8_t* new_ptr = (uint8_t*)realloc(ptr, cap);
      if (!new_ptr) {
        free(ptr);
        result.status_message = "#load_file: out of memory";
        return result;
      }
      ptr = new_ptr;
    }

    size_t n = fread(ptr + len, 1, cap - len, f);
    if (errno == EINTR) {
      num_eintr++;
      if (num_eintr >= 100) {
        free(ptr);
        result.status_message = "#load_file: interrupted read";
        return result;
      }
    } else if (n == 0) {
      break;
    }
    len += n;
  }

  if (ferror(f)) {
    free(ptr);
    result.status_message = "#load_file: invalid file";
    return result;
  }

  result.owned_memory = ptr;
  result.dst_ptr = ptr;
  result.dst_len = len;
  result.truncated = truncated;
  return result;
}

#endif  // INCLUDE_GUARD_ETC
