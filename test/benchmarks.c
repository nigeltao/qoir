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
#include "../util/load_file.c"
#include "../util/walk_directory.c"

// ----

int g_number_of_reps;
int g_verbose;
qoir_decode_buffer g_decbuf;
qoir_encode_buffer g_encbuf;

typedef struct timings_struct {
  uint64_t original_size;
  uint64_t compressed_size;
  uint64_t encode_pixels;
  uint64_t encode_micros;
  uint64_t decode_pixels;
  uint64_t decode_micros;
} timings;

void                    //
print_timings(          //
    timings* t,         //
    const char* name0,  //
    const char* name1,  //
    const char* name2) {
  double cratio = t->compressed_size / ((double)(t->original_size));
  double espeed = t->encode_pixels / ((double)(t->encode_micros));
  double dspeed = t->decode_pixels / ((double)(t->decode_micros));
  printf("%s%6.4f CmpRatio  %8.2f EncMPixels/s  %8.2f DecMPixels/s  %s%s%s\n",
         "QOIR    ", cratio, espeed, dspeed, name0, name1, name2);
}

typedef struct my_context_struct {
  const char* benchname;
  timings timings[WALK_DIRECTORY_MAX_EXCL_DEPTH];
} my_context;

const char*                //
bench_one_pixbuf(          //
    my_context* z,         //
    uint32_t depth,        //
    const char* dirname,   //
    const char* filename,  //
    qoir_pixel_buffer* src_pixbuf) {
  qoir_encode_options encopts = {0};
  encopts.encbuf = &g_encbuf;
  qoir_encode_result enc = qoir_encode(src_pixbuf, &encopts);
  if (enc.status_message) {
    printf("%s%s%s: could not encode QOIR\n", z->benchname, dirname, filename);
    free(enc.owned_memory);
    return enc.status_message;
  }

  uint64_t original_num_bytes =
      ((uint64_t)(src_pixbuf->pixcfg.height_in_pixels)) *
      ((uint64_t)(src_pixbuf->stride_in_bytes));
  uint64_t original_num_pixels =
      ((uint64_t)(src_pixbuf->pixcfg.height_in_pixels)) *
      ((uint64_t)(src_pixbuf->pixcfg.width_in_pixels));
  for (int d = 0; d <= depth; d++) {
    z->timings[d].original_size += original_num_bytes;
    z->timings[d].compressed_size += enc.dst_len;
  }

  {
    struct timeval timeval0;
    gettimeofday(&timeval0, NULL);
    for (int i = 0; i < g_number_of_reps; i++) {
      free(qoir_encode(src_pixbuf, &encopts).owned_memory);
    }
    struct timeval timeval1;
    gettimeofday(&timeval1, NULL);

    int64_t micros = ((int64_t)(timeval1.tv_sec - timeval0.tv_sec)) * 1000000 +
                     ((int64_t)(timeval1.tv_usec - timeval0.tv_usec));
    for (int d = 0; d <= depth; d++) {
      z->timings[d].encode_pixels += g_number_of_reps * original_num_pixels;
      z->timings[d].encode_micros += (micros > 0) ? micros : 1;
    }
  }

  qoir_decode_options decopts = {0};
  decopts.decbuf = &g_decbuf;
  qoir_decode_result dec = qoir_decode(enc.dst_ptr, enc.dst_len, &decopts);
  free(dec.owned_memory);
  if (dec.status_message) {
    printf("%s%s%s: could not decode QOIR\n", z->benchname, dirname, filename);
    free(enc.owned_memory);
    return dec.status_message;
  }

  {
    struct timeval timeval0;
    gettimeofday(&timeval0, NULL);
    for (int i = 0; i < g_number_of_reps; i++) {
      free(qoir_decode(enc.dst_ptr, enc.dst_len, &decopts).owned_memory);
    }
    struct timeval timeval1;
    gettimeofday(&timeval1, NULL);

    int64_t micros = ((int64_t)(timeval1.tv_sec - timeval0.tv_sec)) * 1000000 +
                     ((int64_t)(timeval1.tv_usec - timeval0.tv_usec));
    for (int d = 0; d <= depth; d++) {
      z->timings[d].decode_pixels += g_number_of_reps * original_num_pixels;
      z->timings[d].decode_micros += (micros > 0) ? micros : 1;
    }
  }

  free(enc.owned_memory);
  return NULL;
}

const char*                  //
bench_one_png(               //
    my_context* z,           //
    uint32_t depth,          //
    const char* dirname,     //
    const char* filename,    //
    const uint8_t* src_ptr,  //
    size_t src_len) {
  int width = 0;
  int height = 0;
  int channels = 0;
  if (!stbi_info_from_memory(src_ptr, src_len, &width, &height, &channels)) {
    printf("%s%s%s: could not decode PNG\n", z->benchname, dirname, filename);
    return "stbi_info_from_memory failed";
  } else if (channels != 3) {
    channels = 4;
  }

  int ignored = 0;
  unsigned char* pixbuf_data = stbi_load_from_memory(
      src_ptr, src_len, &ignored, &ignored, &ignored, channels);
  if (!pixbuf_data) {
    printf("%s%s%s: could not decode PNG\n", z->benchname, dirname, filename);
    return "stbi_load_from_memory failed";
  } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
    stbi_image_free(pixbuf_data);
    printf("%s%s%s: image is too large\n", z->benchname, dirname, filename);
    return "stbi_load_from_memory failed";
  }

  qoir_pixel_buffer pixbuf;
  pixbuf.pixcfg.pixfmt = (channels == 3) ? QOIR_PIXEL_FORMAT__RGB
                                         : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  pixbuf.pixcfg.width_in_pixels = width;
  pixbuf.pixcfg.height_in_pixels = height;
  pixbuf.data = (uint8_t*)pixbuf_data;
  pixbuf.stride_in_bytes = (size_t)channels * (size_t)width;
  const char* result = bench_one_pixbuf(z, depth, dirname, filename, &pixbuf);
  stbi_image_free(pixbuf_data);
  return result;
}

const char*          //
my_enter_callback(   //
    void* context,   //
    uint32_t depth,  //
    const char* dirname) {
  my_context* z = (my_context*)context;
  memset(&z->timings[depth], 0, sizeof(z->timings[depth]));
  return NULL;
}

const char*          //
my_exit_callback(    //
    void* context,   //
    uint32_t depth,  //
    const char* dirname) {
  my_context* z = (my_context*)context;
  if (z->timings[depth].original_size > 0) {
    print_timings(&z->timings[depth], z->benchname, dirname, "");
  }
  return NULL;
}

const char*               //
my_file_callback(         //
    void* context,        //
    uint32_t depth,       //
    const char* dirname,  //
    const char* filename) {
  my_context* z = (my_context*)context;
  size_t n = strlen(filename);
  if ((n < 4) || strcmp(filename + n - 4, ".png")) {
    return NULL;
  }

  FILE* f = fopen(filename, "rb");
  if (!f) {
    printf("%s: could not open \"%s\": %s\n", z->benchname, filename,
           strerror(errno));
    return "fopen failed";
  }
  load_file_result r = load_file(f, UINT64_MAX);
  fclose(f);
  const char* result = r.status_message;
  if (!result) {
    memset(&z->timings[depth], 0, sizeof(z->timings[depth]));
    result = bench_one_png(z, depth, dirname, filename, r.dst_ptr, r.dst_len);
    if (g_verbose || (z->benchname[0] == '\x00')) {
      print_timings(&z->timings[depth], z->benchname, dirname, filename);
    }
  }
  free(r.owned_memory);
  return result;
}

int         //
benchmark(  //
    const char* src_filename) {
  my_context z = {0};
  const char* status_message = NULL;

  DIR* d = opendir(src_filename);
  if (d) {
    z.benchname = src_filename;
    status_message = walk_directory(d, &z, &my_enter_callback,
                                    &my_exit_callback, &my_file_callback);
    closedir(d);
  } else {
    struct stat s;
    if (stat(src_filename, &s)) {
      status_message = strerror(errno);
    } else {
      z.benchname = "";
      memset(&z.timings[0], 0, sizeof(z.timings[0]));
      status_message = my_file_callback(&z, 0, "", src_filename);
    }
  }

  if (status_message) {
    printf("could not walk \"%s\": %s\n", src_filename, status_message);
    return 1;
  }
  return 0;
}

// ----

int            //
main(          //
    int argc,  //
    char** argv) {
  g_number_of_reps = 5;
  g_verbose = 0;

  for (int i = 1; i < argc; i++) {
    if (*argv[i] != '-') {
      int result = benchmark(argv[i]);
      if (result) {
        return result;
      }
      continue;
    }

    const char* arg = argv[i] + 1;
    if (*arg == '-') {
      arg++;
    }

    if (!strncmp(arg, "n=", 2)) {
      int x = atoi(arg + 2);
      if (x >= 0) {
        g_number_of_reps = x;
      }
    } else if (!strncmp(arg, "v", 2)) {
      g_verbose = 1;
    } else {
      printf("unsupported argument: %s\n", argv[i]);
      return 1;
    }
  }

  return 0;
}
