// Copyright (C) 2024 Lily Lyons
//
// This file is part of ModShot-BaiHua.
//
// ModShot-BaiHua is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ModShot-BaiHua is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ModShot-BaiHua.  If not, see <https://www.gnu.org/licenses/>.

#include <zmq.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <SDL3/SDL.h>

#include "clover.png.xxd"
#include "journal_common.h"
#include "niko1.png.xxd"
#include "niko2.png.xxd"
#include "niko3.png.xxd"

SDL_HitTestResult hit_test_fun(SDL_Window *window, const SDL_Point *point,
                               void *userdata) {
  SDL_Surface *surf = (SDL_Surface *)userdata;

  unsigned char a = 0;
  SDL_ReadSurfacePixel(surf, point->x, point->y, NULL, NULL, NULL, &a);

  if (a > 10)
    return SDL_HITTEST_DRAGGABLE;
  else
    return SDL_HITTEST_NORMAL;
}

void journal_handling();
void niko_handling(int x, int y);

int main(int argc, char **argv) {
  if (argc == 3) {
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    niko_handling(x, y);
  } else {
    journal_handling();
  }
}

struct Ctx {
  zmq::context_t zmq_ctx;
  zmq::socket_t pub_socket;
  zmq::socket_t sub_socket;

  SDL_Mutex *mutex;
  SDL_Thread *thread;

  SDL_Renderer *renderer;
  SDL_Window *window;
  SDL_Texture *texture;
  SDL_Surface *surface;
  stbi_uc *pixels;

  bool running;

  ~Ctx() {
    SDL_DestroyMutex(mutex);

    SDL_DestroyTexture(texture);
    SDL_DestroyWindow(window);

    SDL_DestroySurface(surface);
    stbi_image_free(pixels);
  }
};

int server_thread(void *data) {
  Ctx *ctx = (Ctx *)data;

  // send a hello over the queue to let oneshot (if it is open) know that a
  // journal has opened
  Message message;
  message.tag = Message::Hello;

  zmq::message_t zmq_message(&message, sizeof(Message));
  ctx->pub_socket.send(zmq_message, zmq::send_flags::none);

  while (ctx->running) {
    try {
      ctx->sub_socket.recv(zmq_message, zmq::recv_flags::none);
    } catch (zmq::error_t &exception) {
      std::cerr << "zmq socket recv error: " << exception.what() << std::endl;
      return 1;
    }

    message = *zmq_message.data<Message>();
    // lock the mutex while we are modifying ctx
    SDL_LockMutex(ctx->mutex);
    switch (message.tag) {
      // if we got a hello, respond with hello
      // (hello is sent when oneshot starts)
      // we dont need to worry about infinite loops here because we get a
      // hello from the message queue, not the status queue
    case Message::Hello: {
      message.tag = Message::Hello;
      zmq_message.rebuild(&message, sizeof(Message));
      ctx->pub_socket.send(zmq_message, zmq::send_flags::none);
    } break;
    // close if we are asked to
    case Message::Close:
      ctx->running = false;
      break;
    // move to a position
    case Message::SetWindowPosition: {
      SDL_SetWindowPosition(ctx->window, message.val.pos.x, message.val.pos.y);
    } break;
    // weve started to recieve an image path
    case Message::ImagePath: {
      std::string string;
      // recieve all chunks of the path
      do {
        string.append(message.val.text.chars, message.val.text.len);

        ctx->sub_socket.recv(zmq_message, zmq::recv_flags::none);
        message = *zmq_message.data<Message>();
        // break loop if we are done
      } while (message.tag == Message::ImagePath);

      // FIXME error handling
      FILE *file = fopen(string.c_str(), "rb");
      if (!file) {
        std::cerr << "failed to open file" << string;
        break;
      }

      SDL_DestroySurface(ctx->surface);
      SDL_DestroyTexture(ctx->texture);
      stbi_image_free(ctx->pixels);

      int w, h, comp;
      ctx->pixels = stbi_load_from_file(file, &w, &h, &comp, 4);

      ctx->surface = SDL_CreateSurfaceFrom(ctx->pixels, w, h, w * 4,
                                           SDL_PIXELFORMAT_ABGR8888);
      ctx->texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->surface);

      SDL_SetWindowSize(ctx->window, w, h);
      SDL_SetWindowHitTest(ctx->window, hit_test_fun, ctx->surface);
    } break;
    default:
      std::cerr << "Unhandled message tag";
      break;
    }
    // unlock the mutex
    SDL_UnlockMutex(ctx->mutex);
  }

  return 0;
}

void journal_handling() {
  // FIXME error handling
  SDL_Init(SDL_INIT_VIDEO);

  Ctx ctx;
  ctx.zmq_ctx = zmq::context_t(1);
  ctx.pub_socket = zmq::socket_t(ctx.zmq_ctx, zmq::socket_type::push);
  ctx.sub_socket = zmq::socket_t(ctx.zmq_ctx, zmq::socket_type::pull);

  ctx.pub_socket.bind("tcp://localhost:7969");
  ctx.sub_socket.connect("tcp://localhost:9697");

  // send a hello over the queue to let oneshot (if it is open) know that a
  // journal has opened
  Message message;
  message.tag = Message::Hello;

  zmq::message_t zmq_message(&message, sizeof(Message));
  ctx.pub_socket.send(zmq_message, zmq::send_flags::dontwait);

  int w, h, comp;
  ctx.pixels = stbi_load_from_memory(
      ___journal_clover_png, ___journal_clover_png_len, &w, &h, &comp, 4);
  ctx.surface =
      SDL_CreateSurfaceFrom(ctx.pixels, w, h, w * 4, SDL_PIXELFORMAT_ABGR8888);

  ctx.window =
      SDL_CreateWindow(" ", w, h, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN);

  ctx.renderer = SDL_CreateRenderer(ctx.window, NULL, SDL_RENDERER_SOFTWARE);

  ctx.texture = SDL_CreateTextureFromSurface(ctx.renderer, ctx.surface);

  SDL_SetWindowHitTest(ctx.window, hit_test_fun, ctx.surface);

  int window_x, window_y, old_window_x, old_window_y = 0;
  SDL_GetWindowPosition(ctx.window, &window_x, &window_y);

  ctx.mutex = SDL_CreateMutex();
  ctx.thread = SDL_CreateThread(server_thread, "server thread", &ctx);

  ctx.running = true;

  while (ctx.running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        ctx.running = false;
        break;
      }
    }

    SDL_LockMutex(ctx.mutex);

    // FIXME this does not account for the position while dragging the window on
    // windows (apparently)
    SDL_GetWindowPosition(ctx.window, &window_x, &window_y);
    if (window_x != old_window_x || window_y != old_window_y) {
      old_window_x = window_x;
      old_window_y = window_y;

      message.tag = Message::WindowPosition;
      message.val.pos = {window_x, window_y};
      zmq_message.rebuild(&message, sizeof(Message));
      ctx.pub_socket.send(zmq_message, zmq::send_flags::dontwait);
    }

    SDL_RenderClear(ctx.renderer);
    SDL_RenderTexture(ctx.renderer, ctx.texture, NULL, NULL);
    SDL_RenderPresent(ctx.renderer);

    SDL_ShowWindow(ctx.window);

    SDL_UnlockMutex(ctx.mutex);

    // calculates to 60 fps
    SDL_Delay(1000 / 60);
  }

  // tell oneshot we have closed
  message.tag = Message::Goodbye;
  zmq_message.rebuild(&message, sizeof(Message));
  ctx.pub_socket.send(zmq_message, zmq::send_flags::dontwait);
}

void niko_handling(int x, int y) {
  // FIXME error handling
  SDL_Init(SDL_INIT_VIDEO);

  int w, h, comp;
  auto niko1_pixels = stbi_load_from_memory(
      ___journal_niko1_png, ___journal_niko1_png_len, &w, &h, &comp, 4);
  auto niko2_pixels = stbi_load_from_memory(
      ___journal_niko2_png, ___journal_niko2_png_len, &w, &h, &comp, 4);
  auto niko3_pixels = stbi_load_from_memory(
      ___journal_niko3_png, ___journal_niko3_png_len, &w, &h, &comp, 4);

  SDL_Window *window = SDL_CreateWindow(
      " ", w, h,
      SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN | SDL_WINDOW_ALWAYS_ON_TOP |
          SDL_WINDOW_UTILITY | SDL_WINDOW_BORDERLESS);
  SDL_SetWindowPosition(window, x, y);

  SDL_Rect screen_rect;
  SDL_DisplayID di = SDL_GetDisplayForWindow(window);
  SDL_GetDisplayUsableBounds(di, &screen_rect);

  SDL_Surface *niko1_surf = SDL_CreateSurfaceFrom(niko1_pixels, w, h, w * 4,
                                                  SDL_PIXELFORMAT_ABGR8888);
  SDL_Surface *niko2_surf = SDL_CreateSurfaceFrom(niko2_pixels, w, h, w * 4,
                                                  SDL_PIXELFORMAT_ABGR8888);
  SDL_Surface *niko3_surf = SDL_CreateSurfaceFrom(niko3_pixels, w, h, w * 4,
                                                  SDL_PIXELFORMAT_ABGR8888);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, NULL, SDL_RENDERER_SOFTWARE);

  SDL_Texture *niko1_tex = SDL_CreateTextureFromSurface(renderer, niko1_surf);
  SDL_Texture *niko2_tex = SDL_CreateTextureFromSurface(renderer, niko2_surf);
  SDL_Texture *niko3_tex = SDL_CreateTextureFromSurface(renderer, niko3_surf);

  for (int niko_offset = 0; niko_offset + y < screen_rect.h + screen_rect.y;
       niko_offset += 2) {
    // discard all os events so the os thinks we are not stuck
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }

    SDL_RenderClear(renderer);

    if (niko_offset % 32 >= 16)
      SDL_RenderTexture(renderer, niko2_tex, NULL, NULL);
    else if (niko_offset / 32 % 2)
      SDL_RenderTexture(renderer, niko1_tex, NULL, NULL);
    else
      SDL_RenderTexture(renderer, niko3_tex, NULL, NULL);

    SDL_RenderPresent(renderer);

    SDL_SetWindowPosition(window, x, niko_offset + y);
    SDL_ShowWindow(window);

    // calculates to 60 fps
    SDL_Delay(1000 / 60);
  }

  SDL_DestroyTexture(niko1_tex);
  SDL_DestroyTexture(niko2_tex);
  SDL_DestroyTexture(niko3_tex);

  SDL_DestroyWindow(window);

  SDL_DestroySurface(niko1_surf);
  SDL_DestroySurface(niko2_surf);
  SDL_DestroySurface(niko3_surf);

  stbi_image_free(niko1_pixels);
  stbi_image_free(niko2_pixels);
  stbi_image_free(niko3_pixels);
}