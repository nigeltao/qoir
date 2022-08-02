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

SDL_Surface*  //
load(const char* filename, void** owned_memory) {
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

  qoir_decode_options opts = {0};
  opts.pixfmt = QOIR_PIXEL_FORMAT__BGRA_PREMUL;
  qoir_decode_result decode = qoir_decode(ptr, len, &opts);
  free(ptr);
  ptr = NULL;
  len = 0;
  if (decode.status_message) {
    free(decode.owned_memory);
    fprintf(stderr, "main: load: could not decode file: %s\n",
            decode.status_message);
    return NULL;
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

bool  //
draw(SDL_Window* window, SDL_Surface* surface) {
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

int  //
main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
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
  SDL_Surface* surface = load(argv[1], &surface_owned_memory);
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
