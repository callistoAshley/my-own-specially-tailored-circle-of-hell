#include <SDL2/SDL.h>
#include "etc-internal.h"
#include "gl-fun.h"
#include "gl-util.h"
#include "gl-meta.h"
#include "quad.h"
#include "graphics.h"
#include "scene.h"
#include "binding-util.h"
#include "sharedstate.h"

struct PingPong {
    TEXFBO rt[2];
    uint8_t srcInd, dstInd;
    int screenW, screenH;
    
    PingPong(int screenW, int screenH)
    : srcInd(0), dstInd(1), screenW(screenW), screenH(screenH) {
        for (int i = 0; i < 2; ++i) {
            TEXFBO::init(rt[i]);
            TEXFBO::allocEmpty(rt[i], screenW, screenH);
            TEXFBO::linkFBO(rt[i]);
            gl.ClearColor(0, 0, 0, 1);
            FBO::clear();
        }
    }
    
    ~PingPong() {
        for (int i = 0; i < 2; ++i)
            TEXFBO::fini(rt[i]);
    }
    
    TEXFBO &backBuffer() { return rt[srcInd]; }
    
    TEXFBO &frontBuffer() { return rt[dstInd]; }
    
    /* Better not call this during render cycles */
    void resize(int width, int height) {
        screenW = width;
        screenH = height;
        
        for (int i = 0; i < 2; ++i)
            TEXFBO::allocEmpty(rt[i], width, height);
    }
    
    void startRender() { bind(); }
    
    void swapRender() {
        std::swap(srcInd, dstInd);
        
        bind();
    }
    
    void clearBuffers() {
        glState.clearColor.pushSet(Vec4(0, 0, 0, 1));
        
        for (int i = 0; i < 2; ++i) {
            FBO::bind(rt[i].fbo);
            FBO::clear();
        }
        
        glState.clearColor.pop();
    }
    
private:
    void bind() { FBO::bind(rt[dstInd].fbo); }
};

class WindowScene : public Scene {
public:
  PingPong pp;
  Quad screenQuad;

  WindowScene(int w, int h): pp(w, h) {
    geometry.rect.w = w;
    geometry.rect.h = h;

    screenQuad.setTexPosRect(geometry.rect, geometry.rect);
  }

  void composite() {
    // would rather not call this but we get segfaults otherwise
    // probably not too prohibitive to call this every frame, most things have a dirty flag
    shState->prepareDraw();

    glState.viewport.set(IntRect(0, 0, geometry.rect.w, geometry.rect.h));

    pp.startRender();

    gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    FBO::clear();

    Scene::composite();
  }

  void resize(int w, int h) {
    geometry.rect.w = w;
    geometry.rect.h = h;

    pp.resize(w, h);
    screenQuad.setTexPosRect(geometry.rect, geometry.rect);

    notifyGeometryChange();
  }

  void requestViewportRender(const Vec4&c, const Vec4&f, const Vec4&t, const bool s, const Vec4 rx, const Vec4 ry, const float cubic) {
    const IntRect &viewpRect = glState.scissorBox.get();
    const IntRect &screenRect = geometry.rect;

    const bool toneRGBEffect = t.xyzNotNull();
    const bool toneGrayEffect = t.w != 0;
    const bool colorEffect = c.w > 0;
    const bool flashEffect = f.w > 0;
    const bool cubicEffect = cubic != 0;
    const bool rgbOffset = rx.xyzNotNull() || ry.xyzNotNull();
    const bool scannedEffect = s;
            
    if (toneGrayEffect) {
        pp.swapRender();
        
        if (!viewpRect.encloses(screenRect)) {
            /* Scissor test _does_ affect FBO blit operations,
             * and since we're inside the draw cycle, it will
             * be turned on, so turn it off temporarily */
            glState.scissorTest.pushSet(false);
            
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(pp.frontBuffer(), false, geometry.rect, pp.backBuffer(), geometry.rect);
            GLMeta::blitBegin(pp.frontBuffer(), false, scaleIsSpecial);
            GLMeta::blitSource(pp.backBuffer(), scaleIsSpecial);
            GLMeta::blitRectangle(geometry.rect, Vec2i());
            GLMeta::blitEnd();
            
            glState.scissorTest.pop();
        }
        
        GrayShader &shader = shState->shaders().gray;
        shader.bind();
        shader.setGray(t.w);
        shader.applyViewportProj();
        shader.setTexSize(screenRect.size());
        
        TEX::bind(pp.backBuffer().tex);
        
        glState.blend.pushSet(false);
        screenQuad.draw();
        glState.blend.pop();
    }

    if (scannedEffect)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      ScannedShader &shader = shState->shaders().scanned;
      shader.bind();
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }

    if (cubicEffect)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      CubicShader &shader = shState->shaders().cubic;
      shader.bind();
      shader.setiTime(cubic);
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }
    if (rgbOffset)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      ChronosShader &shader = shState->shaders().chronos;
      shader.bind();
      shader.setrgbOffset(rx, ry);
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }

    if (!toneRGBEffect && !colorEffect && !flashEffect)
        return;

    FlatColorShader &shader = shState->shaders().flatColor;
    shader.bind();
    shader.applyViewportProj();
            
    if (toneRGBEffect) {
        /* First split up additive / substractive components */
        Vec4 add, sub;
        
        if (t.x > 0)
            add.x = t.x;
        if (t.y > 0)
            add.y = t.y;
        if (t.z > 0)
            add.z = t.z;
          
        if (t.x < 0)
            sub.x = -t.x;
        if (t.y < 0)
            sub.y = -t.y;
        if (t.z < 0)
            sub.z = -t.z;
          
        /* Then apply them using hardware blending */
        gl.BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        
        if (add.xyzNotNull()) {
            gl.BlendEquation(GL_FUNC_ADD);
            shader.setColor(add);
            
            screenQuad.draw();
        }
        
        if (sub.xyzNotNull()) {
            gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            shader.setColor(sub);
            
            screenQuad.draw();
        }
    }

    if (colorEffect || flashEffect) {
        gl.BlendEquation(GL_FUNC_ADD);
        gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                             GL_ONE);
    }

    if (colorEffect) {
        shader.setColor(c);
        screenQuad.draw();
    }

    if (flashEffect) {
        shader.setColor(f);
        screenQuad.draw();
    }

    glState.blendMode.refresh();
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
  GLMeta::blitSource(window->scene.pp.frontBuffer(), 0);

  gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  FBO::clear();

  GLMeta::blitRectangle(IntRect(0, 0, w, h), IntRect(0,h,w,-h), false);

  GLMeta::blitEnd();

  SDL_GL_SwapWindow(window->window); 

  GFX_UNLOCK;

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