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

#ifndef INCLUDE_GUARD_UTIL_WALK_DIRECTORY
#define INCLUDE_GUARD_UTIL_WALK_DIRECTORY

#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define WALK_DIRECTORY_MAX_EXCL_DEPTH 64

typedef struct walk_directory_state_struct {
  void* context;
  const char* (*enter_callback)(void* context,
                                uint32_t depth,
                                const char* dirname);
  const char* (*exit_callback)(void* context,
                               uint32_t depth,
                               const char* dirname);
  const char* (*file_callback)(void* context,
                               uint32_t depth,
                               const char* dirname,
                               const char* filename);
  uint16_t depth;
  bool path_too_long;
  uint32_t path_len;
  char path[4096];
} walk_directory_state;

static const char*  //
walk_directory_1(walk_directory_state* z, DIR* d) {
  int old_cwd = open(".", O_RDONLY, 0);
  if (old_cwd < 0) {
    return "#walk_directory: could not get current working directory";
  }
  fchdir(dirfd(d));
  const char* dirname = z->path_too_long ? "/.../" : z->path;

  const char* result = (*z->enter_callback)(z->context, z->depth, dirname);
  if (result) {
    goto done;
  }

  uint32_t original_path_len = z->path_len;
  while (1) {
    struct dirent* e = readdir(d);
    if (!e) {
      break;
    } else if (e->d_name[0] == '.') {
      continue;
    }

    struct stat s;
    if (stat(e->d_name, &s)) {
      continue;
    } else if (!S_ISDIR(s.st_mode)) {
      result =
          (*z->file_callback)(z->context, z->depth + 1, dirname, e->d_name);
      if (result) {
        goto done;
      }
      continue;
    } else if (z->depth >= (WALK_DIRECTORY_MAX_EXCL_DEPTH - 2)) {
      result = "#walk_directory: too much recursion";
      goto done;
    }

    DIR* subdir = opendir(e->d_name);
    if (!subdir) {
      continue;
    }
    size_t n = strlen(e->d_name);
    bool original_path_too_long = z->path_too_long;
    z->path_too_long = (n + 2) > (sizeof(z->path) - z->path_len);
    if (!z->path_too_long) {
      memcpy(z->path + z->path_len, e->d_name, n);
      z->path_len += n;
      z->path[z->path_len++] = '/';
      z->path[z->path_len] = '\x00';
    }

    z->depth++;
    result = walk_directory_1(z, subdir);
    z->depth--;
    z->path_too_long = original_path_too_long;
    z->path_len = original_path_len;
    z->path[original_path_len] = '\x00';
    closedir(subdir);
    if (result) {
      goto done;
    }
  }

done:
  do {
    const char* exit_result =
        (*z->exit_callback)(z->context, z->depth, dirname);
    fchdir(old_cwd);
    close(old_cwd);
    return result ? result : exit_result;
  } while (0);
}

static const char*  //
walk_directory(DIR* d,
               void* context,
               const char* (*enter_callback)(void* context,
                                             uint32_t depth,
                                             const char* dirname),
               const char* (*exit_callback)(void* context,
                                            uint32_t depth,
                                            const char* dirname),
               const char* (*file_callback)(void* context,
                                            uint32_t depth,
                                            const char* dirname,
                                            const char* filename)) {
  walk_directory_state z;
  z.context = context;
  z.enter_callback = enter_callback;
  z.exit_callback = exit_callback;
  z.file_callback = file_callback;
  z.depth = 0;
  z.path_too_long = false;
  z.path_len = 1;
  z.path[0] = '/';
  z.path[1] = '\x00';
  return walk_directory_1(&z, d);
}

#endif  // INCLUDE_GUARD_ETC
