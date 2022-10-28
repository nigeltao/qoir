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

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
#include <stdio.h>

#define QOIR_IMPLEMENTATION
#include "../src/qoir.h"

// ----

bool g_multithreaded = false;
bool g_print_decode_time = false;

// ----

typedef struct worker_data_struct {
  // Request.
  const uint8_t* src_ptr;
  size_t src_len;
  qoir_decode_options* options;
  int32_t y0;
  int32_t y1;

  // Response.
  const char* status_message;
} worker_data;

int    //
work(  //
    void* data) {
  worker_data* wd = (worker_data*)data;
  qoir_decode_options opts = {0};
  memcpy(&opts, wd->options, sizeof(*wd->options));
  if (!opts.pixbuf.data) {
    wd->status_message =
        "main: inconsistent qoir_decode_options.pixbuf.data field\n";
    return 0;
  }
  if (opts.src_clip_rectangle.y0 < wd->y0) {
    opts.src_clip_rectangle.y0 = wd->y0;
  }
  if (opts.src_clip_rectangle.y1 > wd->y1) {
    opts.src_clip_rectangle.y1 = wd->y1;
  }

  qoir_decode_result res = qoir_decode(wd->src_ptr, wd->src_len, &opts);
  if (res.owned_memory) {
    free(res.owned_memory);
    wd->status_message =
        "main: inconsistent qoir_decode_result.owned_memory field\n";
  } else {
    wd->status_message = res.status_message;
  }
  return 0;
}

qoir_decode_result           //
multithreaded_decode(        //
    const uint8_t* src_ptr,  //
    const size_t src_len,    //
    const qoir_decode_options* options) {
  qoir_decode_pixel_configuration_result config =
      qoir_decode_pixel_configuration(src_ptr, src_len);
  if (config.status_message) {
    qoir_decode_result res = {0};
    res.status_message = config.status_message;
    return res;
  }

  uint32_t height_in_tiles =
      qoir_calculate_number_of_tiles_1d(config.dst_pixcfg.height_in_pixels);
  if (height_in_tiles <= 1) {
    return qoir_decode(src_ptr, src_len, options);
  }

  uint64_t pixbuf_len = 4 * (uint64_t)config.dst_pixcfg.width_in_pixels *
                        (uint64_t)config.dst_pixcfg.height_in_pixels;
  if (pixbuf_len > SIZE_MAX) {
    qoir_decode_result res = {0};
    res.status_message =
        qoir_status_message__error_unsupported_pixbuf_dimensions;
    return res;
  }

  qoir_decode_options opts = {0};
  if (options) {
    memcpy(&opts, options, sizeof(*options));
  }

  opts.pixbuf.pixcfg.pixfmt = QOIR_PIXEL_FORMAT__BGRA_PREMUL;
  opts.pixbuf.pixcfg.width_in_pixels = config.dst_pixcfg.width_in_pixels;
  opts.pixbuf.pixcfg.height_in_pixels = config.dst_pixcfg.height_in_pixels;
  opts.pixbuf.data = malloc(pixbuf_len);
  opts.pixbuf.stride_in_bytes = 4 * opts.pixbuf.pixcfg.width_in_pixels;
  if (!opts.pixbuf.data) {
    qoir_decode_result res = {0};
    res.status_message = qoir_status_message__error_out_of_memory;
    return res;
  }

  if (!opts.use_src_clip_rectangle) {
    opts.use_src_clip_rectangle = true;
    opts.src_clip_rectangle =
        qoir_make_rectangle(0, 0, (int32_t)config.dst_pixcfg.width_in_pixels,
                            (int32_t)config.dst_pixcfg.height_in_pixels);
  }

  const char* status_message = NULL;
  uint32_t num_threads = height_in_tiles;
  if (num_threads > 16) {
    num_threads = 16;
  }
  worker_data data[16] = {0};
  SDL_Thread* threads[16] = {0};

  for (uint32_t i = 0; i < num_threads; i++) {
    data[i].src_ptr = src_ptr;
    data[i].src_len = src_len;
    data[i].options = &opts;
    data[i].y0 = (((i + 0) * height_in_tiles) / num_threads) * QOIR_TILE_SIZE;
    data[i].y1 = (((i + 1) * height_in_tiles) / num_threads) * QOIR_TILE_SIZE;
    threads[i] = SDL_CreateThread(&work, "worker", &data[i]);
    if (!threads[i]) {
      status_message = "main: could not create thread";
    }
  }

  for (uint32_t i = 0; i < num_threads; i++) {
    if (threads[i]) {
      SDL_WaitThread(threads[i], NULL);
      if (!status_message) {
        status_message = data[i].status_message;
      }
    }
  }
  if (status_message) {
    free(opts.pixbuf.data);
    qoir_decode_result res = {0};
    res.status_message = status_message;
    return res;
  }
  qoir_decode_result res = {0};
  res.owned_memory = opts.pixbuf.data;
  memcpy(&res.dst_pixbuf, &opts.pixbuf, sizeof(opts.pixbuf));
  return res;
}

// ----

SDL_Surface*               //
load(                      //
    const char* filename,  //
    void** owned_memory) {
  SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
  if (!rw) {
    fprintf(stderr, "main: load: SDL_RWFromFile: %s\n", SDL_GetError());
    return NULL;
  }

  int64_t len = rw->size(rw);
  if (len < 0) {
    rw->close(rw);
    fprintf(stderr, "main: load: could not get file size\n");
    return NULL;
  } else if (len == 0) {
    rw->close(rw);
    fprintf(stderr, "main: load: empty file\n");
    return NULL;
  } else if (len > 0x7FFFFFFF) {
    rw->close(rw);
    fprintf(stderr, "main: load: file is too large\n");
    return NULL;
  }

  void* ptr = malloc(len);
  if (!ptr) {
    rw->close(rw);
    fprintf(stderr, "main: load: out of memory\n");
    return NULL;
  }
  size_t n = rw->read(rw, ptr, 1, len);
  rw->close(rw);
  rw = NULL;
  if (n < len) {
    free(ptr);
    fprintf(stderr, "main: load: could not read file\n");
    return NULL;
  }

  uint64_t now = SDL_GetPerformanceCounter();
  qoir_decode_options opts = {0};
  opts.pixfmt = QOIR_PIXEL_FORMAT__BGRA_PREMUL;
  qoir_decode_result decode = g_multithreaded
                                  ? multithreaded_decode(ptr, len, &opts)
                                  : qoir_decode(ptr, len, &opts);
  free(ptr);
  ptr = NULL;
  len = 0;
  if (decode.status_message) {
    free(decode.owned_memory);
    fprintf(stderr, "main: load: could not decode file: %s\n",
            decode.status_message);
    return NULL;
  }
  if (g_print_decode_time) {
    uint64_t elapsed = SDL_GetPerformanceCounter() - now;
    printf("%" PRIu64 " microseconds to decode.\n",
           (elapsed * 1000000) / SDL_GetPerformanceFrequency());
  }

  *owned_memory = decode.owned_memory;
  return SDL_CreateRGBSurfaceFrom(
      decode.dst_pixbuf.data, decode.dst_pixbuf.pixcfg.width_in_pixels,
      decode.dst_pixbuf.pixcfg.height_in_pixels, 32,
      decode.dst_pixbuf.stride_in_bytes,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF);
#else
      0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
#endif
}

bool                     //
draw(                    //
    SDL_Window* window,  //
    SDL_Surface* surface) {
  SDL_Surface* ws = SDL_GetWindowSurface(window);
  if (!ws) {
    // Use an indirect approach, for exotic window pixel formats (e.g. X.org 10
    // bits per RGB channel), when we can't get the window surface directly.
    //
    // See https://github.com/google/wuffs/issues/51
    SDL_Renderer* r = SDL_CreateRenderer(window, -1, 0);
    if (!r) {
      fprintf(stderr, "main: draw: SDL_CreateRenderer: %s\n", SDL_GetError());
      return false;
    }
    SDL_RenderClear(r);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_RenderCopy(r, texture, NULL, &surface->clip_rect);
    SDL_DestroyTexture(texture);
    SDL_RenderPresent(r);
    SDL_DestroyRenderer(r);
    return true;
  }

  // Use a direct approach.
  SDL_FillRect(ws, NULL, SDL_MapRGB(ws->format, 0x00, 0x00, 0x00));
  SDL_BlitSurface(surface, NULL, ws, NULL);
  SDL_UpdateWindowSurface(window);
  return true;
}

int            //
main(          //
    int argc,  //
    char** argv) {
  const char* filename = NULL;
  bool too_many_args = false;
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if ((arg[0] == '-') && (arg[1] == '-')) {
      arg++;
    }
    if (strcmp(arg, "-multithreaded") == 0) {
      g_multithreaded = true;
      continue;
    } else if (strcmp(arg, "-print-decode-time") == 0) {
      g_print_decode_time = true;
      continue;
    } else if (filename == NULL) {
      filename = argv[i];
      continue;
    }
    too_many_args = true;
  }

  if (too_many_args || (filename == NULL)) {
    fprintf(stderr, "usage: %s -print-decode-time -multithreaded filename\n",
            argv[0]);
    return 1;
  }
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "main: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window =
      SDL_CreateWindow("qoirview", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "main: SDL_CreateWindow: %s\n", SDL_GetError());
    return 1;
  }
  void* surface_owned_memory = NULL;
  SDL_Surface* surface = load(filename, &surface_owned_memory);
  if (!surface) {
    return 1;
  }

  while (true) {
    SDL_Event event;
    if (!SDL_WaitEvent(&event)) {
      fprintf(stderr, "main: SDL_WaitEvent: %s\n", SDL_GetError());
      return 1;
    }

    switch (event.type) {
      case SDL_QUIT:
        goto cleanup;

      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_EXPOSED:
            if (!draw(window, surface)) {
              return 1;
            }
            break;
        }
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            goto cleanup;
        }
        break;
    }
  }

cleanup:
  SDL_FreeSurface(surface);
  free(surface_owned_memory);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
