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
#include "../util/load_file.c"
#include "../util/pixbufs_are_equal.c"

// ----

int  //
do_test_round_trip_5(const char* testname,
                     const char* src_filename,
                     qoir_pixel_buffer* src_pixbuf,
                     uint8_t* enc_ptr,
                     size_t enc_len) {
  qoir_decode_options opts = {0};
  opts.pixfmt = src_pixbuf->pixcfg.pixfmt;
  qoir_decode_result dec = qoir_decode(enc_ptr, enc_len, &opts);
  if (dec.status_message) {
    printf("%s: decode \"%s\": %s\n", testname, src_filename,
           dec.status_message);
    free(dec.owned_memory);
    return 1;
  } else if (!pixbufs_are_equal(src_pixbuf, &dec.dst_pixbuf)) {
    printf("%s: \"%s\": round trip produced different pixels\n", testname,
           src_filename);
    free(dec.owned_memory);
    return 1;
  }
  free(dec.owned_memory);
  return 0;
}

int  //
do_test_round_trip_4(const char* testname,
                     const char* src_filename,
                     qoir_pixel_buffer* src_pixbuf) {
  qoir_encode_result enc = qoir_encode(src_pixbuf, NULL);
  if (enc.status_message) {
    printf("%s: encode \"%s\": %s\n", testname, src_filename,
           enc.status_message);
    free(enc.owned_memory);
    return 1;
  }

  int result = do_test_round_trip_5(testname, src_filename, src_pixbuf,
                                    enc.dst_ptr, enc.dst_len);
  free(enc.owned_memory);
  return result;
}

int  //
do_test_round_trip_3(const char* testname,
                     const char* src_filename,
                     const uint8_t* src_ptr,
                     size_t src_len) {
  for (int channels = 3; channels <= 4; channels++) {
    int width = 0;
    int height = 0;
    unsigned char* pixbuf_data = stbi_load_from_memory(src_ptr, src_len, &width,
                                                       &height, NULL, channels);
    if (!pixbuf_data) {
      printf("%s: could not decode \"%s\"\n", testname, src_filename);
      return 1;
    } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
      stbi_image_free(pixbuf_data);
      printf("%s: \"%s\" is too large\n", testname, src_filename);
      return 1;
    }
    qoir_pixel_buffer src_pixbuf;
    src_pixbuf.pixcfg.pixfmt = (channels == 3)
                                   ? QOIR_PIXEL_FORMAT__RGB
                                   : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
    src_pixbuf.pixcfg.width_in_pixels = width;
    src_pixbuf.pixcfg.height_in_pixels = height;
    src_pixbuf.data = (uint8_t*)pixbuf_data;
    src_pixbuf.stride_in_bytes = (size_t)channels * (size_t)width;

    int result = do_test_round_trip_4(testname, src_filename, &src_pixbuf);
    stbi_image_free(pixbuf_data);
    if (result) {
      return result;
    }
  }
  return 0;
}

int  //
do_test_round_trip_2(const char* testname, const char* src_filename, FILE* f) {
  load_file_result r = load_file(f, UINT64_MAX);
  if (r.status_message) {
    printf("%s: could not read \"%s\": %s\n", testname, src_filename,
           r.status_message);
    free(r.owned_memory);
    return 1;
  }

  int result =
      do_test_round_trip_3(testname, src_filename, r.dst_ptr, r.dst_len);
  free(r.owned_memory);
  return result;
}

int  //
do_test_round_trip_1(const char* testname, const char* src_filename) {
  FILE* f = fopen(src_filename, "r");
  if (!f) {
    printf("%s: could not open \"%s\": %s\n", testname, src_filename,
           strerror(errno));
    return 1;
  }

  int result = do_test_round_trip_2(testname, src_filename, f);
  fclose(f);
  return result;
}

int  //
test_round_trip(void) {
  if (do_test_round_trip_1(__func__, "test/data/bricks-color.png") ||
      do_test_round_trip_1(__func__, "test/data/harvesters.png") ||
      do_test_round_trip_1(__func__, "test/data/hibiscus.primitive.png") ||
      do_test_round_trip_1(__func__, "test/data/hibiscus.regular.png")) {
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
