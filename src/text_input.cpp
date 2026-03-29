#include "text_input.h"
#include "drawing.h"

TextInput textInput;

void TextInput::begin(u8 limit, const std::string& cset, char exitCh) {
  buffer.clear();
  maxLen = limit;
  cursor = 0;
  charIndex = 0;
  charset = cset;
  charsetLength = charset.length();
  done = false;
  exitChar = exitCh;
}

void TextInput::update() {
  if (done) return;

  if (btnLeft()) {
    charIndex--;
    if (charIndex < 0) charIndex = charsetLength - 1;
  }

  if (btnRight()) {
    charIndex++;
    if (charIndex >= charsetLength) charIndex = 0;
  }

  if (btnCenter()) {
    char picked = charset[charIndex];

    if (picked == exitChar) {
      if (cursor == 0) return;
      done = true;
      return;
    }

    buffer += picked;
    cursor++;
    charIndex = 0;

    if (cursor >= maxLen) {
      done = true;
    }
  }
}

void TextInput::draw(const std::string& label) {
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(2, 16, label.c_str());

  u8g2.setFont(u8g2_font_4x6_tr);
  std::string cap = std::to_string(cursor) + "/" + std::to_string(maxLen);
  u8g2.drawStr(108, 8, cap.c_str());

  u8g2.setFont(u8g2_font_oskool_tr);
  i16 fieldY = 32;

  char currentChar = charset[charIndex];
  std::string display = buffer + currentChar;

  i16 tw = u8g2.getStrWidth(display.c_str());
  i16 fieldX = (128 - tw) / 2;
  if (fieldX < 4) fieldX = 4;

  if (cursor > 0) {
    u8g2.drawStr(fieldX, fieldY, buffer.c_str());
  }

  std::string single(1, currentChar);
  if (single[0] == ' ') single[0] = '_';
  if (single[0] == '#') single = "OK";
  i16 beforeW = (cursor > 0) ? u8g2.getStrWidth(buffer.c_str()) : 0;
  i16 pickX = fieldX + beforeW;

  if ((millis() / 400) % 2 == 0) {
    u8g2.drawStr(pickX, fieldY, single.c_str());
  }

  i16 charW = u8g2.getStrWidth(single.c_str());
  u8g2.drawLine(pickX, fieldY + 2, pickX + charW - 1, fieldY + 2);

  u8g2.setFont(u8g2_font_oskool_tr);
  i16 stripY = 48;
  const i8 visible = 9;

  for (i8 offset = -(visible / 2); offset <= (visible / 2); offset++) {
    i8 idx = charIndex + offset;
    while (idx < 0) idx += charsetLength;
    while (idx >= charsetLength) idx -= charsetLength;

    std::string ch(1, charset[idx]);
    if (ch[0] == ' ') ch[0] = '_';
    if (ch[0] == '#') ch = "OK";

    i16 px = 64 + (offset * 12) - 3;
    if (px < 0 || px > 122) continue;

    if (offset == 0) {
      i16 boxW = u8g2.getStrWidth(ch.c_str()) + 4;
      u8g2.drawFrame(px - 2, stripY - 10, boxW, 13);
    }
    u8g2.drawStr(px, stripY, ch.c_str());
  }
}