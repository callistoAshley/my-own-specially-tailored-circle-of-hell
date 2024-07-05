#include <SDL2/SDL.h>
#include "etc-internal.h"
#include "gl-fun.h"
#include "gl-util.h"
#include "gl-meta.h"
#include "texpool.h"
#include "graphics.h"
#include "scene.h"
#include "binding-util.h"
#include "sharedstate.h"


class WindowScene : public Scene {
public:
  TEXFBO tex;

  WindowScene() {
    tex = shState->texPool().request(480, 480);
  }

  void composite() {
    glState.viewport.set(IntRect(0, 0, 480, 480));

    FBO::bind(tex.fbo);

    gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    
    FBO::clear();

    Scene::composite();
  }

  void requestViewportRender(const Vec4& color, const Vec4& flash, const Vec4& tone, const bool scanned, const Vec4 rbg, const Vec4 rbg2, const float cubic) {
    // do nothing
  }
};

struct ScreenWindow {
  SDL_Window* window;
  WindowScene scene;

  ScreenWindow();
  ~ScreenWindow();

  Scene* getScene();
};

ScreenWindow::ScreenWindow(){
  window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 480, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
}
ScreenWindow::~ScreenWindow() {
  if (window)
    SDL_DestroyWindow(window);
}

Scene* ScreenWindow::getScene() {
  return &scene;
}

DEF_TYPE_CUSTOMNAME(ScreenWindow, "Screen::Window");

#define GUARD_DISPOSED(w) if (!w->window) rb_raise(rb_eRuntimeError, "Window already disposed!");

RB_METHOD(screenWindowInit) {
  ScreenWindow* w = new ScreenWindow();

  setPrivateData(self, w);

  return self;
}

RB_METHOD(screenWindowDispose) {
  ScreenWindow* w = getPrivateData<ScreenWindow>(self);

  delete w;
  setPrivateData(self, nullptr);

  return Qnil;
}

RB_METHOD(screenWindowDraw) {
  ScreenWindow* w = getPrivateData<ScreenWindow>(self);

  GUARD_DISPOSED(w);

  SDL_GLContext ctx = shState->graphics().context();
  int err = SDL_GL_MakeCurrent(w->window, ctx);
  if (err != 0) {
    rb_raise(rb_eRuntimeError, "Failed to make window current: %s", SDL_GetError());
  }

  w->scene.composite();

  GLMeta::blitBeginScreen(Vec2i(480, 480), false);
  GLMeta::blitSource(w->scene.tex, 0);

  FBO::clear();

  GLMeta::blitRectangle(IntRect(0, 0, 480, 480), IntRect(0,480,480,-480), false);

  GLMeta::blitEnd();

  SDL_GL_SwapWindow(w->window); 

  return Qnil;
}


void osfmBindingInit() {
  VALUE module = rb_define_module("Screen");

  VALUE klass = rb_define_class_under(module, "Window", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&ScreenWindowType>);

  _rb_define_method(klass, "initialize", screenWindowInit);
  _rb_define_method(klass, "dispose", screenWindowDispose);
  _rb_define_method(klass, "draw", screenWindowDraw);
}