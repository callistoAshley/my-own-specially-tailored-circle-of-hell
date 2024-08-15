#include "binding-types.h"
#include "binding-util.h"
#include "debugwriter.h"
#include "i18n.h"

#include "journal_common.h"
#include "sharedstate.h"

#include <SDL3/SDL.h>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

#include <zmq.hpp>

static SDL_Thread *thread = NULL;
static SDL_Mutex *mutex = NULL;

static zmq::context_t *zmq_ctx = NULL;
static zmq::socket_t *pub_socket = NULL;
static zmq::socket_t *sub_socket = NULL;

static volatile bool active = false;
static volatile int journal_x = 0;
static volatile int journal_y = 0;

int server_thread(void *data) {

  // ask any currently open journals to send a hello
  Message message;
  message.tag = Message::Hello;
  zmq::message_t zmq_message(sizeof(Message));

  pub_socket->send(zmq_message, zmq::send_flags::none);

  // set active if any journals responded with hello
  for (;;) {
    try {
      sub_socket->recv(zmq_message, zmq::recv_flags::none);
    } catch (zmq::error_t &exception) {
      std::cerr << "zmq socket recv error: " << exception.what() << std::endl;
      return 1;
    }
    message = *zmq_message.data<Message>();

    switch (message.tag) {
    case Message::Hello:
      active = true;
      break;
    case Message::WindowPosition:
      journal_x = message.val.pos.x;
      journal_y = message.val.pos.y;
      break;
    case Message::Goodbye:
      active = false;
      break;
    default:
      Debug() << "Unhandled message tag";
      break;
    }
  }

  return 0;
}

RB_METHOD(journalSet) {
  RB_UNUSED_PARAM;
  const char *name;
  rb_get_args(argc, argv, "z", &name RB_ARG_END);

  // if the journal is not active, return
  if (!active)
    return Qnil;

  Message message;
  zmq::message_t zmq_message(sizeof(Message));

  // replicate old journal behaviour where calling set() with "" closes the
  // journal
  if (strlen(name) == 0) {
    message.tag = Message::Close;
    zmq_message.rebuild(&message, sizeof(Message));
    pub_socket->send(zmq_message, zmq::send_flags::none);

    active = false;

    return Qnil;
  }

  auto pwd = std::filesystem::current_path();
  std::string dir = pwd.string();

  dir += "/Graphics/Journal/";
  dir += name;
  dir += ".png";

  // we have to chunk the image path
  message.tag = Message::ImagePath;
  // for i in ceil(dir.length() / 24)
  for (unsigned int i = 0; i < (dir.length() + 23) / 24; i++) {
    std::string substr = dir.substr(i * 24, 24);

    // copy substring into message
    message.val.text.len = substr.length();
    strncpy(message.val.text.chars, substr.c_str(), substr.length());

    // send message
    zmq_message.rebuild(&message, sizeof(Message));
    pub_socket->send(zmq_message, zmq::send_flags::sndmore);
  }

  // tell the journal we are finished sending the image path
  message.tag = Message::FinishImagePath;
  zmq_message.rebuild(&message, sizeof(Message));
  pub_socket->send(zmq_message, zmq::send_flags::none);

  return Qnil;
}

RB_METHOD(journalSetLang) {
  RB_UNUSED_PARAM;
  const char *lang;
  rb_get_args(argc, argv, "z", &lang RB_ARG_END);

  // FIXME language handling

  return Qnil;
}

RB_METHOD(journalActive) {
  RB_UNUSED_PARAM;
  return active ? Qtrue : Qfalse;
}

RB_METHOD(journalPosition) {
  RB_UNUSED_PARAM;

  if (!active)
    return Qnil;

  return rb_ary_new_from_args(2, INT2FIX(journal_x), RB_INT2FIX(journal_y));
}

RB_METHOD(setJournalPosition) {
  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y);

  Message message;
  message.tag = Message::SetWindowPosition;
  message.val.pos = {x, y};
  zmq::message_t zmq_message(&message, sizeof(Message));

  pub_socket->send(zmq_message, zmq::send_flags::none);

  return Qnil;
}

RB_METHOD(journalQuit) {
  Message message;
  message.tag = Message::Close;
  zmq::message_t zmq_message(&message, sizeof(Message));

  pub_socket->send(zmq_message, zmq::send_flags::none);

  active = false;

  return Qnil;
}

void cleanup_journal_stuff() {
  zmq_ctx->shutdown();
  delete pub_socket;
  delete sub_socket;
  delete zmq_ctx;
}

void oneshotJournalBindingInit() {
  mutex = SDL_CreateMutex();

  zmq_ctx = new zmq::context_t(1);
  pub_socket = new zmq::socket_t(*zmq_ctx, zmq::socket_type::push);
  sub_socket = new zmq::socket_t(*zmq_ctx, zmq::socket_type::pull);

  pub_socket->bind("tcp://localhost:9697");
  sub_socket->connect("tcp://localhost:7969");

  thread = SDL_CreateThread(server_thread, "journal server thread", NULL);

  VALUE module = rb_define_module("Journal");
  _rb_define_module_function(module, "set", journalSet);
  _rb_define_module_function(module, "active?", journalActive);
  _rb_define_module_function(module, "setLang", journalSetLang);
  _rb_define_module_function(module, "journal_position", journalPosition);
  _rb_define_module_function(module, "set_journal_position",
                             setJournalPosition);
  _rb_define_module_function(module, "quit", journalQuit);
}
