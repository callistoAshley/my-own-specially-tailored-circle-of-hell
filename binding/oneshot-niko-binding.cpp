#include "binding-util.h"
#include "eventthread.h"
#include "sharedstate.h"

#define NIKO_X (320 - 16)
#define NIKO_Y ((13 * 16) * 2)

#include <filesystem>

#ifdef __WIN32__
#include <shlwapi.h>
#endif
#ifdef __LINUX__
#include <cstdlib>
#endif
#include <sstream>

RB_METHOD(nikoPrepare) {
  RB_UNUSED_PARAM;

  // Blank

  return Qnil;
}

int niko_process_fun() { return 0; }

RB_METHOD(nikoStart) {
  RB_UNUSED_PARAM;

  // Calculate where to stick the window
  // Top-left area of client (hopefully)
  int x, y;
  SDL_GetWindowPosition(shState->rtData().window, &x, &y);
  x += NIKO_X;
  y += NIKO_Y;

  // there was a bunch of pipe junk that is not used at all, so that is all
  // removed

  auto pwd = std::filesystem::current_path();
  std::string dir = pwd.string();

#ifdef __WIN32__
  dir += "_______.exe";
  std::stringstream args(dir);
  args << " " << x << " " << y;

  bool result = CreateProcessA(dir.c_str(), (LPSTR)args.str().c_str(), NULL,
                               NULL, false, 0, NULL, NULL, NULL, NULL);
  if (!result) {
    DWORD dwLastError = GetLastError();

    Debug() << "Failed to start process" << dir;
    Debug() << "Win32 Error Code:" << dwLastError;
  }
#endif
#ifdef __LINUX__
  dir += "/_______";

  std::stringstream args = std::stringstream();
  args << "\"" << dir << "\" " << x << " " << y << " &";

  system(args.str().c_str());
#endif

  return Qnil;
}

void oneshotNikoBindingInit() {
  VALUE module = rb_define_module("Niko");

  // Niko:: module functions
  _rb_define_module_function(module, "get_ready", nikoPrepare);
  _rb_define_module_function(module, "do_your_thing", nikoStart);
}
