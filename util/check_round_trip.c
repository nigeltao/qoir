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

#ifndef INCLUDE_GUARD_UTIL_CHECK_ROUND_TRIP
#define INCLUDE_GUARD_UTIL_CHECK_ROUND_TRIP

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "./load_file.c"
#include "./pixbufs_are_equal.c"

// ----

const char*  //
check_round_trip_3(qoir_pixel_buffer* src_pixbuf,
                   uint8_t* enc_ptr,
                   size_t enc_len) {
  qoir_decode_options opts = {0};
  opts.pixfmt = src_pixbuf->pixcfg.pixfmt;
  qoir_decode_result dec = qoir_decode(enc_ptr, enc_len, &opts);
  if (dec.status_message) {
    free(dec.owned_memory);
    return dec.status_message;
  } else if (!pixbufs_are_equal(src_pixbuf, &dec.dst_pixbuf)) {
    free(dec.owned_memory);
    return "#check_round_trip: round trip produced different pixels";
  }
  free(dec.owned_memory);
  return NULL;
}

const char*  //
check_round_trip_2(qoir_pixel_buffer* src_pixbuf) {
  qoir_encode_result enc = qoir_encode(src_pixbuf, NULL);
  if (enc.status_message) {
    free(enc.owned_memory);
    return enc.status_message;
  }
  const char* result = check_round_trip_3(src_pixbuf, enc.dst_ptr, enc.dst_len);
  free(enc.owned_memory);
  return result;
}

const char*  //
check_round_trip_1(const uint8_t* src_ptr, size_t src_len) {
  for (int channels = 3; channels <= 4; channels++) {
    int width = 0;
    int height = 0;
    unsigned char* pixbuf_data = stbi_load_from_memory(src_ptr, src_len, &width,
                                                       &height, NULL, channels);
    if (!pixbuf_data) {
      return "#check_round_trip: STBI could not decode image";
    } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
      stbi_image_free(pixbuf_data);
      return "#check_round_trip: image is too large";
    }
    qoir_pixel_buffer src_pixbuf;
    src_pixbuf.pixcfg.pixfmt = (channels == 3)
                                   ? QOIR_PIXEL_FORMAT__RGB
                                   : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
    src_pixbuf.pixcfg.width_in_pixels = width;
    src_pixbuf.pixcfg.height_in_pixels = height;
    src_pixbuf.data = (uint8_t*)pixbuf_data;
    src_pixbuf.stride_in_bytes = (size_t)channels * (size_t)width;

    const char* result = check_round_trip_2(&src_pixbuf);
    stbi_image_free(pixbuf_data);
    if (result) {
      return result;
    }
  }
  return NULL;
}

const char*  //
check_round_trip(FILE* f) {
  load_file_result r = load_file(f, UINT64_MAX);
  if (r.status_message) {
    free(r.owned_memory);
    return r.status_message;
  }
  const char* result = check_round_trip_1(r.dst_ptr, r.dst_len);
  free(r.owned_memory);
  return result;
}

#endif  // INCLUDE_GUARD_ETC
