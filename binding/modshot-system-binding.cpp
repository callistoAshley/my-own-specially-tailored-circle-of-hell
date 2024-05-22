#include "binding-types.h"
#include "binding-util.h"
#include "etc.h"
#include "eventthread.h"
#include "oneshot.h"
#include "sharedstate.h"

#ifdef _WIN32
#include <windows.h>
#endif

RB_METHOD(GetScreenResolution) {

  int di = SDL_GetWindowDisplayIndex(shState->rtData().window);
  SDL_Rect rect;
  SDL_GetDisplayUsableBounds(di, &rect);

  Rect *rb_rect = new Rect(rect.x, rect.y, rect.w, rect.h);
  return wrapObject(rb_rect, RectType);
}

void modshotSystemBindingInit() {
  VALUE module = rb_define_module("System");
  _rb_define_module_function(module, "GetScreenResolution",
                             GetScreenResolution);
}