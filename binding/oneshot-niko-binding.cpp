#include "binding-util.h"
#include "eventthread.h"
#include "sharedstate.h"
#include "debugwriter.h"
#include <cstdio>

#define NIKO_X (320 - 16)
#define NIKO_Y ((13 * 16) * 2)

#include <filesystem>
#ifdef __WIN32__
#include <process.h>
#else
#include <unistd.h>
#endif

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
  dir += "\\_______.exe";
#endif
#ifdef __LINUX__
  dir += "/_______";
#endif

  std::string window_x = std::to_string(x);
  std::string window_y = std::to_string(y);
  char* const args[] = {
      const_cast<char*>(dir.c_str()), const_cast<char*>(window_x.c_str()),
      const_cast<char*>(window_y.c_str())};

#ifdef __WIN32__
  spawnv(_P_DETACH, dir.c_str(), args);
#else
  pid_t pid = fork();
  if (pid == 0) {
    execv(dir.c_str(), args);
  }
#endif

  return Qnil;
}

void oneshotNikoBindingInit() {
  VALUE module = rb_define_module("Niko");

  // Niko:: module functions
  _rb_define_module_function(module, "get_ready", nikoPrepare);
  _rb_define_module_function(module, "do_your_thing", nikoStart);
}
