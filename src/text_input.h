#pragma once

#include "types.h"
#include "buttons.h"

// charset for name
const std::string CHARSET_ALPHA = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const u8 CHARSET_ALPHA_LEN = CHARSET_ALPHA.length();

// charset for age '#' is the confirm character
const std::string CHARSET_NUMERIC = "#0123456789";
const u8 CHARSET_NUMERIC_LEN = CHARSET_NUMERIC.length();

struct TextInput {
  std::string buffer;
  u8 maxLen;
  u8 cursor;
  i8 charIndex;
  std::string charset;
  u8 charsetLength;
  bool done;
  char exitChar;

  void begin(u8 limit, const std::string& cset, char exitCh = ' ');
  void update();
  void draw(const std::string& label);
};

extern TextInput textInput;