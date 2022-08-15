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

#define QOIR_IMPLEMENTATION
#include "../src/qoir.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "../third_party/stb/stb_image.h"
#include "../third_party/stb/stb_image_write.h"

// Utilities should be #include'd after qoir.h
#include "../util/load_file.c"

// ----

load_file_result  //
convert_from_png_to_qoir(const uint8_t* src_ptr,
                         size_t src_len,
                         const qoir_encode_options* enc_opts) {
  load_file_result result = {0};

  int width = 0;
  int height = 0;
  int channels = 0;
  if (!stbi_info_from_memory(src_ptr, src_len, &width, &height, &channels)) {
    result.status_message = "#main: stbi_info_from_memory failed";
    return result;
  } else if (channels != 3) {
    channels = 4;
  }
  int ignored = 0;
  unsigned char* pixbuf_data = stbi_load_from_memory(
      src_ptr, src_len, &ignored, &ignored, &ignored, channels);
  if (!pixbuf_data) {
    result.status_message = "#main: stbi_load_from_memory failed";
    return result;
  } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
    stbi_image_free(pixbuf_data);
    result.status_message = "#main: image is too large";
    return result;
  }

  qoir_pixel_buffer pixbuf;
  pixbuf.pixcfg.pixfmt = (channels == 3) ? QOIR_PIXEL_FORMAT__RGB
                                         : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  pixbuf.pixcfg.width_in_pixels = width;
  pixbuf.pixcfg.height_in_pixels = height;
  pixbuf.data = (uint8_t*)pixbuf_data;
  pixbuf.stride_in_bytes = (size_t)channels * (size_t)width;
  qoir_encode_result enc = qoir_encode(&pixbuf, enc_opts);
  stbi_image_free(pixbuf_data);
  if (enc.status_message) {
    free(enc.owned_memory);
    result.status_message = enc.status_message;
    return result;
  }

  result.owned_memory = enc.owned_memory;
  result.dst_ptr = enc.dst_ptr;
  result.dst_len = enc.dst_len;
  return result;
}

// ----

void  //
my_stbi_write_func(void* context, void* data, int size) {
  load_file_result* result = (load_file_result*)context;
  if (!result || (size <= 0)) {
    return;
  }
  result->owned_memory = malloc(size);
  if (!result->owned_memory) {
    result->status_message = "#main: out of memory";
    return;
  }
  memcpy(result->owned_memory, data, size);
  result->dst_ptr = result->owned_memory;
  result->dst_len = size;
}

load_file_result  //
convert_from_qoir_to_png(const uint8_t* src_ptr, size_t src_len) {
  load_file_result result = {0};

  qoir_decode_pixel_configuration_result cfg =
      qoir_decode_pixel_configuration(src_ptr, src_len);
  if (cfg.status_message) {
    result.status_message = cfg.status_message;
    return result;
  }

  qoir_decode_options decopts = {0};
  decopts.pixfmt = (cfg.dst_pixcfg.pixfmt == QOIR_PIXEL_FORMAT__BGRX)
                       ? QOIR_PIXEL_FORMAT__RGB
                       : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  qoir_decode_result dec = qoir_decode(src_ptr, src_len, &decopts);
  if (dec.status_message) {
    free(dec.owned_memory);
    result.status_message = dec.status_message;
    return result;
  }

  if (!stbi_write_png_to_func(
          &my_stbi_write_func, &result, dec.dst_pixbuf.pixcfg.width_in_pixels,
          dec.dst_pixbuf.pixcfg.height_in_pixels,
          qoir_pixel_format__bytes_per_pixel(decopts.pixfmt),
          dec.dst_pixbuf.data, dec.dst_pixbuf.stride_in_bytes)) {
    free(dec.owned_memory);
    result.status_message = "#main: stbi_write_png failed";
    return result;
  }

  free(dec.owned_memory);
  return result;
}

// ----

int  //
usage() {
  fprintf(stderr,
          "Usage:\n"                                     //
          "  qoirconv --lossiness=L foo.png foo.qoir\n"  //
          "  qoirconv foo.qoir foo.png\n"                //
          "  L ranges in 0 ..= 7; the default (0) means lossless\n");
  return 1;
}

int  //
main(int argc, char** argv) {
  const char* arg_src = NULL;
  const char* arg_dst = NULL;
  qoir_encode_options encopts = {0};

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (arg[0] != '-') {
      if (!arg_src) {
        arg_src = arg;
        continue;
      } else if (!arg_dst) {
        arg_dst = arg;
        continue;
      }
      return usage();
    }
    arg++;
    if (!strncmp(arg, "-lossiness=", 11)) {
      long int x = strtol(arg + 11, NULL, 10);
      if ((0 <= x) && (x < 8)) {
        encopts.lossiness = x;
        continue;
      }
    }
    return usage();
  }

  FILE* fin = stdin;
  if (arg_src) {
    fin = fopen(arg_src, "rb");
    if (!fin) {
      fprintf(stderr, "qoirconv: could not open %s: %s\n", arg_src,
              strerror(errno));
      return 1;
    }
  }

  load_file_result r0 = load_file(fin, UINT64_MAX);
  if (r0.status_message) {
    fprintf(stderr, "qoirconv: could not load %s: %s\n",
            arg_src ? arg_src : "<stdin>", r0.status_message);
    return 1;
  }

  // We would normally have to call free(r0.owned_memory) at some point, but
  // we're in the main function, so just exit the process (and 'leak' the
  // memory) when this function returns. Ditto for r1.owned_memory below.

  load_file_result r1 = {0};
  r1.status_message = "#main: unsupported file format";
  if (r0.dst_len > 0) {
    switch (r0.dst_ptr[0]) {
      case 0x51:
        r1 = convert_from_qoir_to_png(r0.dst_ptr, r0.dst_len);
        break;
      case 0x89:
        r1 = convert_from_png_to_qoir(r0.dst_ptr, r0.dst_len, &encopts);
        break;
    }
  }
  if (r1.status_message) {
    fprintf(stderr, "qoirconv: could not convert %s: %s\n",
            arg_src ? arg_src : "<stdin>", r1.status_message);
    return 1;
  }

  FILE* fout = stdout;
  if (arg_dst) {
    fout = fopen(arg_dst, "wb");
    if (!fout) {
      fprintf(stderr, "qoirconv: could not open %s: %s\n", arg_dst,
              strerror(errno));
      return 1;
    }
  }
  if (r1.dst_len != fwrite(r1.dst_ptr, 1, r1.dst_len, fout)) {
    fprintf(stderr, "qoirconv: could not save %s: %s\n",
            arg_dst ? arg_dst : "<stdout>", strerror(errno));
    return 1;
  }

  return 0;
}
