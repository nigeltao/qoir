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

const char error_not_implemented[] =  //
    "#main: not implemented";

#if defined(CONFIG_FULL_BENCHMARKS)
#include "../adapter/all_adapters.h"
#endif

// ----

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int g_number_of_reps;
int g_verbose;

typedef struct timings_struct {
  uint64_t original_size;
  uint64_t compressed_size;
  uint64_t encode_pixels;
  uint64_t encode_micros;
  uint64_t decode_pixels;
  uint64_t decode_micros;
} timings;

void               //
add_timings(       //
    timings* dst,  //
    const timings* src) {
  dst->original_size += src->original_size;
  dst->compressed_size += src->compressed_size;
  dst->encode_pixels += src->encode_pixels;
  dst->encode_micros += src->encode_micros;
  dst->decode_pixels += src->decode_pixels;
  dst->decode_micros += src->decode_micros;
}

void                         //
print_timings(               //
    timings* t,              //
    const char* formatname,  //
    const char* name0,       //
    const char* name1,       //
    const char* name2) {
  static double nan = 0.0 / 0.0;
  double cratio = t->compressed_size / ((double)(t->original_size));
  double espeed = t->encode_micros
                      ? (t->encode_pixels / ((double)(t->encode_micros)))
                      : nan;
  double dspeed = t->decode_micros
                      ? (t->decode_pixels / ((double)(t->decode_micros)))
                      : nan;
#if defined(CONFIG_FULL_BENCHMARKS)
  printf(
      "%-16s%6.4f CmpRatio  %8.2f EncMPixels/s  %8.2f DecMPixels/s  %s%s%s\n",
      formatname, cratio, espeed, dspeed, name0, name1, name2);
#else
  printf("%-8s%6.4f CmpRatio  %8.2f EncMPixels/s  %8.2f DecMPixels/s  %s%s%s\n",
         formatname, cratio, espeed, dspeed, name0, name1, name2);
#endif
}

typedef struct timings_result_struct {
  const char* status_message;
  timings value;
} timings_result;

static timings_result       //
make_timings_result_error(  //
    const char* status_message) {
  timings_result result = {0};
  result.status_message = status_message;
  return result;
}

// ----

static qoir_decode_result    //
my_decode_qoir(              //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  // static avoids an allocation every time this function is called, but it
  // means that this function is not thread-safe.
  static qoir_decode_buffer decbuf;

  qoir_decode_options decopts = {0};
  decopts.decbuf = &decbuf;
  return qoir_decode(src_ptr, src_len, &decopts);
}

static qoir_encode_result    //
my_encode_qoir_lossless(     //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  // static avoids an allocation every time this function is called, but it
  // means that this function is not thread-safe.
  static qoir_encode_buffer encbuf;

  qoir_encode_options encopts = {0};
  encopts.encbuf = &encbuf;
  return qoir_encode(src_pixbuf, &encopts);
}

#if defined(CONFIG_FULL_BENCHMARKS)
static qoir_encode_result    //
my_encode_qoir_lossy(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  // static avoids an allocation every time this function is called, but it
  // means that this function is not thread-safe.
  static qoir_encode_buffer encbuf;

  qoir_encode_options encopts = {0};
  encopts.encbuf = &encbuf;
  encopts.lossiness = 2;
  return qoir_encode(src_pixbuf, &encopts);
}
#endif

typedef struct format_struct {
  const char* name;
  qoir_decode_result (*decode_func)(const uint8_t* src_ptr,
                                    const size_t src_len);
  qoir_encode_result (*encode_func)(const uint8_t* png_ptr,
                                    const size_t png_len,
                                    qoir_pixel_buffer* src_pixbuf);
} format;

format my_formats[] = {
#if defined(CONFIG_FULL_BENCHMARKS)
    {"JXL_Lossless/f", &my_decode_jxl_lib, &my_encode_jxl_lossless_fst},
    {"JXL_Lossless/l", &my_decode_jxl_lib, &my_encode_jxl_lossless_lib},
    {"JXL_Lossy/l", &my_decode_jxl_lib, &my_encode_jxl_lossy_lib},
    {"PNG/fpng", &my_decode_png_fpng, &my_encode_png_fpng},
    {"PNG/fpnge", &my_decode_png_fpnge, &my_encode_png_fpnge},
    {"PNG/libpng", &my_decode_png_libpng, &my_encode_png_libpng},
    {"PNG/stb", &my_decode_png_stb, &my_encode_png_stb},
    {"PNG/wuffs", &my_decode_png_wuffs, &my_encode_png_wuffs},
    {"QOI", &my_decode_qoi, &my_encode_qoi},
    {"QOIR_Lossless", &my_decode_qoir, &my_encode_qoir_lossless},
    {"QOIR_Lossy", &my_decode_qoir, &my_encode_qoir_lossy},
    {"WebP_Lossless", &my_decode_webp, &my_encode_webp_lossless},
    {"WebP_Lossy", &my_decode_webp, &my_encode_webp_lossy},
#else
    {"QOIR", &my_decode_qoir, &my_encode_qoir_lossless},
#endif
};

#define MAX_INCL_NUMBER_OF_FORMATS 16

static inline size_t  //
number_of_formats() {
  size_t n = ARRAY_SIZE(my_formats);
  return (n < MAX_INCL_NUMBER_OF_FORMATS) ? n : MAX_INCL_NUMBER_OF_FORMATS;
}

// ----

static timings_result        //
encode_decode(               //
    const char* benchname,   //
    const char* dirname,     //
    const char* filename,    //
    const format* f,         //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  timings_result result = {0};

  const uint8_t* enc_ptr = NULL;
  size_t enc_len = 0;
  qoir_encode_result enc = (*f->encode_func)(png_ptr, png_len, src_pixbuf);
  if (enc.status_message == error_not_implemented) {
    enc_ptr = png_ptr;
    enc_len = png_len;
  } else if (enc.status_message) {
    printf("%s%s%s: could not encode %s\n", benchname, dirname, filename,
           f->name);
    free(enc.owned_memory);
    return make_timings_result_error(enc.status_message);
  } else {
    enc_ptr = enc.dst_ptr;
    enc_len = enc.dst_len;
  }

  uint64_t original_num_bytes =
      ((uint64_t)(src_pixbuf->pixcfg.height_in_pixels)) *
      ((uint64_t)(src_pixbuf->stride_in_bytes));
  uint64_t original_num_pixels =
      ((uint64_t)(src_pixbuf->pixcfg.height_in_pixels)) *
      ((uint64_t)(src_pixbuf->pixcfg.width_in_pixels));
  result.value.original_size = original_num_bytes;
  result.value.compressed_size = enc_len;

  {
    struct timeval timeval0;
    gettimeofday(&timeval0, NULL);
    for (int i = 0; i < g_number_of_reps; i++) {
      free((*f->encode_func)(png_ptr, png_len, src_pixbuf).owned_memory);
    }
    struct timeval timeval1;
    gettimeofday(&timeval1, NULL);

    int64_t micros = ((int64_t)(timeval1.tv_sec - timeval0.tv_sec)) * 1000000 +
                     ((int64_t)(timeval1.tv_usec - timeval0.tv_usec));
    result.value.encode_pixels = g_number_of_reps * original_num_pixels;
    result.value.encode_micros = (enc.status_message == error_not_implemented)
                                     ? 0
                                     : ((micros > 0) ? micros : 1);
  }

  qoir_decode_result dec = (*f->decode_func)(enc_ptr, enc_len);
  free(dec.owned_memory);
  if (dec.status_message == error_not_implemented) {
    // No-op.
  } else if (dec.status_message) {
    printf("%s%s%s: could not decode %s\n", benchname, dirname, filename,
           f->name);
    free(enc.owned_memory);
    return make_timings_result_error(dec.status_message);
  } else {
    struct timeval timeval0;
    gettimeofday(&timeval0, NULL);
    for (int i = 0; i < g_number_of_reps; i++) {
      free((*f->decode_func)(enc_ptr, enc_len).owned_memory);
    }
    struct timeval timeval1;
    gettimeofday(&timeval1, NULL);

    int64_t micros = ((int64_t)(timeval1.tv_sec - timeval0.tv_sec)) * 1000000 +
                     ((int64_t)(timeval1.tv_usec - timeval0.tv_usec));
    result.value.decode_pixels = g_number_of_reps * original_num_pixels;
    result.value.decode_micros = (micros > 0) ? micros : 1;
  }

  free(enc.owned_memory);
  return result;
}

// ----

typedef struct my_context_struct {
  const char* benchname;
  timings timings[MAX_INCL_NUMBER_OF_FORMATS][WALK_DIRECTORY_MAX_EXCL_DEPTH];
} my_context;

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

  const char* ret = NULL;
  for (size_t i = 0; i < number_of_formats(); i++) {
    timings_result t = encode_decode(z->benchname, dirname, filename,
                                     &my_formats[i], src_ptr, src_len, &pixbuf);
    if (t.status_message) {
      ret = t.status_message;
      break;
    }
    for (int d = 0; d <= depth; d++) {
      add_timings(&z->timings[i][d], &t.value);
    }
  }
  stbi_image_free(pixbuf_data);
  return ret;
}

const char*          //
my_enter_callback(   //
    void* context,   //
    uint32_t depth,  //
    const char* dirname) {
  my_context* z = (my_context*)context;
  for (size_t i = 0; i < number_of_formats(); i++) {
    memset(&z->timings[i][depth], 0, sizeof(z->timings[i][depth]));
  }
  return NULL;
}

const char*          //
my_exit_callback(    //
    void* context,   //
    uint32_t depth,  //
    const char* dirname) {
  my_context* z = (my_context*)context;
  for (size_t i = 0; i < number_of_formats(); i++) {
    if (z->timings[i][depth].original_size > 0) {
      print_timings(&z->timings[i][depth], my_formats[i].name, z->benchname,
                    dirname, "");
    }
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
    for (size_t i = 0; i < number_of_formats(); i++) {
      memset(&z->timings[i][depth], 0, sizeof(z->timings[i][depth]));
    }
    result = bench_one_png(z, depth, dirname, filename, r.dst_ptr, r.dst_len);
    if (g_verbose || (z->benchname[0] == '\x00')) {
      for (size_t i = 0; i < number_of_formats(); i++) {
        print_timings(&z->timings[i][depth], my_formats[i].name, z->benchname,
                      dirname, filename);
      }
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

  if (ARRAY_SIZE(my_formats) > MAX_INCL_NUMBER_OF_FORMATS) {
    printf("too many formats\n");
    return 1;
  }

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
