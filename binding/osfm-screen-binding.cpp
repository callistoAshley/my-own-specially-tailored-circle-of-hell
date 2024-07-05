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

  WindowScene(int w, int h) {
    geometry.rect.w = w;
    geometry.rect.h = h;
    tex = shState->texPool().request(w, h);
  }

  void composite() {
    // would rather not call this but we get segfaults otherwise
    // probably not too prohibitive to call this every frame, most things have a dirty flag
    shState->prepareDraw();

    glState.viewport.set(IntRect(0, 0, geometry.rect.w, geometry.rect.h));

    FBO::bind(tex.fbo);

    gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    
    FBO::clear();

    Scene::composite();
  }

  void resize(int w, int h) {
    geometry.rect.w = w;
    geometry.rect.h = h;

    shState->texPool().release(tex);
    tex = shState->texPool().request(w, h);

    notifyGeometryChange();
  }

  void requestViewportRender(const Vec4& color, const Vec4& flash, const Vec4& tone, const bool scanned, const Vec4 rbg, const Vec4 rbg2, const float cubic) {
    // do nothing
  }
};

struct ScreenWindow {
  SDL_Window* window;
  WindowScene scene;

  ScreenWindow(int x, int y, int w, int h, unsigned int flags) : scene(w, h) {
    window = SDL_CreateWindow("Test", x, y, w, h, flags);
  }
  ~ScreenWindow() {
    if (window)
      SDL_DestroyWindow(window);
  }

  Scene* getScene();
};
// so we can use this from outside this file
Scene* ScreenWindow::getScene() {
  return &scene;
}

DEF_TYPE_CUSTOMNAME(ScreenWindow, "Screen::Window");

#define GUARD_DISPOSED(w) if (!w->window) rb_raise(rb_eRuntimeError, "Window already disposed!");

RB_METHOD(screenWindowInit) {
  VALUE vx, vy, vw, vh;
  VALUE kwargs;
  rb_scan_args(argc, argv, "4:", &vx, &vy, &vw, &vh, &kwargs);

  int x = NUM2INT(vx);
  int y = NUM2INT(vy);
  int w = NUM2INT(vw);
  int h = NUM2INT(vh);

  unsigned int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT;
  if (!NIL_P(kwargs)) {
    ID table[4] = {
      rb_intern("borderless"),
      rb_intern("hidden"),
      rb_intern("always_on_top"),
      rb_intern("fullscreen")
    };
    VALUE values[4];

    rb_get_kwargs(kwargs, table, 0, 4, values);

    if (!RTEST(values[0]))
      flags ^= SDL_WINDOW_BORDERLESS; // enabled by default
    if (RTEST(values[1]))
      flags |= SDL_WINDOW_HIDDEN; // shown by default
    if (RTEST(values[2]))
      flags |= SDL_WINDOW_ALWAYS_ON_TOP; // not always on top by default
    if (RTEST(values[3]))
      flags |= SDL_WINDOW_FULLSCREEN; // not fullscreen by default
  }

  ScreenWindow* window = new ScreenWindow(x, y, w, h, flags);

  setPrivateData(self, window);

  return self;
}

RB_METHOD(screenWindowDispose) {
  ScreenWindow* w = getPrivateData<ScreenWindow>(self);

  delete w;
  setPrivateData(self, nullptr);

  return Qnil;
}

RB_METHOD(screenWindowDraw) {
  ScreenWindow* window = getPrivateData<ScreenWindow>(self);

  GFX_LOCK;

  GUARD_DISPOSED(window);

  SDL_GLContext ctx = shState->graphics().context();
  int err = SDL_GL_MakeCurrent(window->window, ctx);
  if (err != 0) {
    GFX_UNLOCK;
    rb_raise(rb_eRuntimeError, "Failed to make window current: %s", SDL_GetError());
  }

  window->scene.composite();
  auto geo = window->scene.getGeometry();
  int w = geo.rect.w;
  int h = geo.rect.h;

  GLMeta::blitBeginScreen(Vec2i(w ,h), false);
  GLMeta::blitSource(window->scene.tex, 0);

  gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  FBO::clear();

  GLMeta::blitRectangle(IntRect(0, 0, w, h), IntRect(0,h,w,-h), false);

  GLMeta::blitEnd();

  SDL_GL_SwapWindow(window->window); 

  GFX_UNLOCK;

  return Qnil;
}

RB_METHOD(screenWindowResize) {
  ScreenWindow* window = getPrivateData<ScreenWindow>(self);

  GUARD_DISPOSED(window);

  int w, h;
  rb_get_args(argc, argv, "ii", &w, &h RB_ARG_END);

  window->scene.resize(w, h);

  SDL_SetWindowSize(window->window, w, h);

  GFX_UNLOCK;

  return Qnil;
}

RB_METHOD(screenWindowMove) {
  ScreenWindow* window = getPrivateData<ScreenWindow>(self);

  GUARD_DISPOSED(window);

  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y RB_ARG_END);

  SDL_SetWindowPosition(window->window, x, y);

  return Qnil;
}

void osfmBindingInit() {
  VALUE module = rb_define_module("Screen");

  VALUE klass = rb_define_class_under(module, "Window", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&ScreenWindowType>);

  _rb_define_method(klass, "initialize", screenWindowInit);
  _rb_define_method(klass, "dispose", screenWindowDispose);
  _rb_define_method(klass, "draw", screenWindowDraw);
  _rb_define_method(klass, "resize", screenWindowResize);
  _rb_define_method(klass, "move", screenWindowMove);

  rb_define_const(module, "UNDEFINED_POS", INT2NUM(SDL_WINDOWPOS_UNDEFINED));
  rb_define_const(module, "CENTERED_POS", INT2NUM(SDL_WINDOWPOS_CENTERED));

}