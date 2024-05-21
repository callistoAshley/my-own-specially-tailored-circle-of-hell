#ifndef JOURNAL_COMMON_H
#define JOURNAL_COMMON_H

struct Message {
  enum {
    Hello,             // the journal or game has connected
    Goodbye,           // the journal or game has disconnected
    Close,             // game requested the journal to close
    WindowPosition,    // game requested the window position of the journal
    SetWindowPosition, // game requested the journal to set its position
    ImagePath,         // send image path
    FinishImagePath    // finished sending image path
  } tag;

  union {
    struct {
      int x, y;
    } pos;

    struct {
      char chars[24];
      unsigned char len;
    } text;
  } val;
};

#endif