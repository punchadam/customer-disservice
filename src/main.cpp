#include <Arduino.h>
#include <string>
#include <stdint.h>
#include <U8g2lib.h>
 
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
 
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
 
const std::string version = "Version 1.2.0";
 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);
 
// butt stuff lol
 
const u8 PIN_LEFT = D10;
const u8 PIN_CENTER = D9;
const u8 PIN_RIGHT = D0;
 
struct Button {
  u8 pin;
  bool state;
  bool lastState;
  bool pressed;
  u32 lastChange;
};
 
Button buttons[3];
const u32 DEBOUNCE_MS = 50;
 
void initButtons() {
  u8 pins[] = { PIN_LEFT, PIN_CENTER, PIN_RIGHT };
  for (u8 i = 0; i < 3; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    buttons[i] = { pins[i], false, false, false, 0 };
  }
}
 
void readButtons() {
  u32 now = millis();
  for (u8 i = 0; i < 3; i++) {
    bool raw = digitalRead(buttons[i].pin);
    buttons[i].pressed = false;
    if (raw != buttons[i].lastState) {
      buttons[i].lastChange = now;
    }
    if ((now - buttons[i].lastChange) > DEBOUNCE_MS) {
      if (raw != buttons[i].state) {
        buttons[i].state = raw;
        if (buttons[i].state) {
          buttons[i].pressed = true;
        }
      }
    }
    buttons[i].lastState = raw;
  }
}
 
bool btnLeft() { return buttons[0].pressed; }
bool btnCenter() { return buttons[1].pressed; }
bool btnRight() { return buttons[2].pressed; }
 
// drawing help
 
const i16 BOX_X = 0;
const i16 BOX_Y = 12;
const i16 BOX_W = 128;
const i16 BOX_H = 52;
 
const u8 CHAR_W = 10;
const u8 CHAR_H = 14;
 
void drawCharacterBox() {
  u8g2.drawFrame(BOX_X, BOX_Y, BOX_W, BOX_H);
}

void drawTextCenteredHorizontally(const std::string& txt, const uint8_t *font, i16 height) {
  u8g2.setFont(font);
  i16 tw = u8g2.getStrWidth(txt.c_str());
  u8g2.drawStr((128 - tw) / 2, height, txt.c_str());
}

 
void drawMenu(const std::string& label, const std::string& value, i8 index, i8 maxIndex) {
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(2, 10, label.c_str());
  u8g2.setFont(u8g2_font_oskool_tr);
  drawTextCenteredHorizontally(value, u8g2_font_oskool_tr, 63);
 
  if (index > 0) {
    u8g2.setFont(u8g2_font_oskool_tr);
    u8g2.drawStr(BOX_X + 4, 63, "<");
  }
  if (index < maxIndex - 1) {
    u8g2.setFont(u8g2_font_oskool_tr);
    u8g2.drawStr(BOX_X + BOX_W - 10, 63, ">");
  }
 
  // u8g2.setFont(u8g2_font_4x6_tr);
  // u8g2.drawStr(2, 63, "[<]prev  [o]ok  [>]next");
}
 
enum class Hat : u8 {
  None = 0,
  Frog,
  Top,
  Baseball,
  Cowboy,
  COUNT
};
 
enum class Death : u8 {
  Fire = 0,
  Fall,
  Crush,
  COUNT
};
 
const std::string hatNames[] = {
  "None", "Frog Hat", "Top Hat", "Baseball Cap", "Cowboy Hat"
};
 
const std::string deathNames[] = {
  "FIRE", "FALL", "CRUSH"
};
 
struct Character {
  std::string name;
  u8 age;
  Hat hat = Hat::None;
  Death death;
 
  // position stuff for gyroscope physics and all dat
  float x;
  float y;
  float vx;
  float vy;
};
 
Character player;
 
void drawCharacter(i16 cx, i16 cy) {
  i16 headY = cy - CHAR_H + 3;
  u8g2.drawCircle(cx, headY, 3);
 
  i16 torsoTop = headY + 3;
  i16 torsoBot = torsoTop + 5;
  u8g2.drawLine(cx, torsoTop, cx, torsoBot);
 
  u8g2.drawLine(cx - 4, torsoTop + 2, cx + 4, torsoTop + 2);
 
  u8g2.drawLine(cx, torsoBot, cx - 3, cy);
  u8g2.drawLine(cx, torsoBot, cx + 3, cy);
 
  switch (player.hat) {
    case Hat::None:
      break;
    case Hat::Frog:
      u8g2.drawFilledEllipse(cx - 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
      u8g2.drawFilledEllipse(cx + 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
      break;
    case Hat::Top:
      u8g2.drawBox(cx - 3, headY - 8, 7, 5);
      u8g2.drawLine(cx - 5, headY - 3, cx + 5, headY - 3);
      break;
    case Hat::Baseball:
      u8g2.drawFilledEllipse(cx, headY - 2, 3, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
      u8g2.drawLine(cx + 3, headY - 2, cx + 6, headY - 2);
      break;
    case Hat::Cowboy:
      u8g2.drawLine(cx - 6, headY - 2, cx + 6, headY - 2);
      u8g2.drawBox(cx - 2, headY - 6, 5, 4);
      u8g2.setDrawColor(0);
      u8g2.drawPixel(cx, headY - 6);
      u8g2.setDrawColor(1);
      break;
    default:
      break;
  }
}
 
// text input
 
// charset for name
const std::string CHARSET_ALPHA = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const u8 CHARSET_ALPHA_LEN = CHARSET_ALPHA.length();
 
// charset for age
const std::string CHARSET_NUMERIC = "0123456789";
const u8 CHARSET_NUMERIC_LEN = CHARSET_NUMERIC.length();
 
struct TextInput {
  std::string buffer;
  u8 maxLen;        // character limit for this field
  u8 cursor;        // current editing position
  i8 charIndex;     // index into the active charset
  std::string charset;
  u8 charsetLength;
  bool done;
 
  void begin(u8 limit, const std::string& cset) {
    buffer.clear();
    maxLen = limit;
    cursor = 0;
    charIndex = 0;
    charset = cset;
    charsetLength = charset.length();
    done = false;
  }
 
  void update() {
    if (done) return;
 
    // scroll left through charset
    if (btnLeft()) {
      charIndex--;
      if (charIndex < 0) charIndex = charsetLength - 1;
    }
 
    // scroll right through charset
    if (btnRight()) {
      charIndex++;
      if (charIndex >= charsetLength) charIndex = 0;
    }
    if (btnCenter()) {
      char picked = charset[charIndex];
 
      // Space = "done" signal (don't allow empty input)
      if (picked == ' ') {
        if (cursor == 0) return;  // nothing typed yet, ignore
        done = true;
        return;
      }
 
      // append the character
      buffer += picked;
      cursor++;
      charIndex = 0;  // reset picker for next position
 
      // hit the max length and auto-finish
      if (cursor >= maxLen) {
        done = true;
      }
    }
  }
 
  void draw(const std::string& label) {

    u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
    u8g2.drawStr(2, 16, label.c_str());
 
    u8g2.setFont(u8g2_font_4x6_tr);
    std::string cap = std::to_string(cursor) + "/" + std::to_string(maxLen);
    u8g2.drawStr(108, 8, cap.c_str());
 
    // text field
    u8g2.setFont(u8g2_font_oskool_tr);
    i16 fieldY = 32;
 
    // build display string: entered text + current pick character
    std::string display = buffer + charset[charIndex];
 
    i16 tw = u8g2.getStrWidth(display.c_str());
    i16 fieldX = (128 - tw) / 2;
    if (fieldX < 4) fieldX = 4;
 
    // draw entered portion
    if (cursor > 0) {
      u8g2.drawStr(fieldX, fieldY, buffer.c_str());
    }
 
    // draw current pick character blinking
    std::string single(1, charset[charIndex]);
    if (single[0] == ' ') single[0] = '_';
    i16 beforeW = (cursor > 0) ? u8g2.getStrWidth(buffer.c_str()) : 0;
    i16 pickX = fieldX + beforeW;
 
    if ((millis() / 400) % 2 == 0) {
      u8g2.drawStr(pickX, fieldY, single.c_str());
    }
 
    // underline at cursor position
    i16 charW = u8g2.getStrWidth(single.c_str());
    u8g2.drawLine(pickX, fieldY + 2, pickX + charW - 1, fieldY + 2);
 
    // shows surrounding characters in the charset
    u8g2.setFont(u8g2_font_oskool_tr);
    i16 stripY = 48;
    const i8 visible = 9;
 
    for (i8 offset = -(visible / 2); offset <= (visible / 2); offset++) {
      i8 idx = charIndex + offset;
      while (idx < 0) idx += charsetLength;
      while (idx >= charsetLength) idx -= charsetLength;
 
      std::string ch(1, charset[idx]);
      if (ch[0] == ' ') ch[0] = '_';  // show space as underscore
 
      i16 px = 64 + (offset * 12) - 3;
      if (px < 0 || px > 122) continue;
 
      if (offset == 0) {
        // box around the selected character
        u8g2.drawFrame(px - 2, stripY - 10, 12, 13);
      }
      u8g2.drawStr(px, stripY, ch.c_str());
    }
    // u8g2.setFont(u8g2_font_4x6_tr);
    // u8g2.drawStr(2, 63, "[<]scroll [o]pick [_]=done");
  }
};
 
TextInput textInput;
 
// game state stuff
 
enum class State : u8 {
  Title,
  ChooseName,
  ChooseAge,
  ChooseHat,
  ChooseDeath,
  Preview,
  Animate,
  Result
};
 
State gameState = State::Title;
u32 stateEnterTime = 0;
i8 menuIndex = 0;
 
void enterState(State s) {
  gameState = s;
  stateEnterTime = millis();
  menuIndex = 0;
}
 
void updateTitle() {
  u8g2.setFont(u8g2_font_oskool_tr);
  u8g2.drawStr(0, 16, "Violet's");
  std::string customer = "CUSTOMER";
  std::string disservice = "DISSERVICE";
  drawTextCenteredHorizontally(customer, u8g2_font_disrespectfulteenager_tu, 32);
  drawTextCenteredHorizontally(disservice, u8g2_font_disrespectfulteenager_tu, 50);

  std::string start = "press any button";
  drawTextCenteredHorizontally(start, u8g2_font_4x6_tr, 63);
 
  if (btnLeft() || btnCenter() || btnRight()) {
    textInput.begin(8, CHARSET_ALPHA);
    enterState(State::ChooseName);
  }
}
 
void updateSelectName() {
  textInput.update();
 
  if (textInput.done) {
    player.name = textInput.buffer;
    textInput.begin(3, CHARSET_NUMERIC);
    enterState(State::ChooseAge);
    return;
  }
 
  textInput.draw("NAME:");
}
 
void updateSelectAge() {
  textInput.update();
 
  if (textInput.done) {
    player.age = (u8)atoi(textInput.buffer.c_str());
    if (player.age == 0) player.age = 1;
    enterState(State::ChooseHat);
    return;
  }
 
  textInput.draw("AGE:");
}
 
void updateSelectHat() {
  i8 maxI = (i8)Hat::COUNT;
  if (btnLeft()  && menuIndex > 0) menuIndex--;
  if (btnRight() && menuIndex < maxI - 1) menuIndex++;
  if (btnCenter()) {
    player.hat = (Hat)menuIndex;
    enterState(State::ChooseDeath);
    return;
  }
  drawMenu("HAT:", hatNames[menuIndex], menuIndex, maxI);
}
 
void updateSelectDeath() {
  i8 maxI = (i8)Death::COUNT;
  if (btnLeft()  && menuIndex > 0) menuIndex--;
  if (btnRight() && menuIndex < maxI - 1) menuIndex++;
  if (btnCenter()) {
    player.death = (Death)menuIndex;
    enterState(State::Preview);
    return;
  }
  drawMenu("DOOM:", deathNames[menuIndex], menuIndex, maxI);
}
 
void updatePreview() {
  drawCharacterBox();
 
  u8g2.setFont(u8g2_font_4x6_tr);
  std::string info = player.name + ", " + std::to_string(player.age);
  u8g2.drawStr(2, 8, info.c_str());
 
  player.x = BOX_X + BOX_W / 2;
  player.y = BOX_Y + BOX_H - 2;
  player.vx = 0;
  player.vy = 0;
 
  drawCharacter((i16)player.x, (i16)player.y);
 
  if (btnLeft() || btnCenter() || btnRight()) {
    enterState(State::Animate);
  }
}
 
void updateAnimate() {
  drawCharacterBox();
 
  u32 elapsed = millis() - stateEnterTime;
 
  // placeholder: simple gravity fall
  float t = elapsed / 1000.0f;
  player.y = (BOX_Y + BOX_H / 2) + (t * t * 30.0f);
 
  if (player.y > BOX_Y + BOX_H - 2) {
    player.y = BOX_Y + BOX_H - 2;
  }
 
  drawCharacter((i16)player.x, (i16)player.y);
 
  if (elapsed > 3000) {
    enterState(State::Result);
  }
}
 
void updateResult() {
  std::string rip = "R.I.P.";
  drawTextCenteredHorizontally(rip, u8g2_font_disrespectfulteenager_tu, 20);

  std::string epitaph = player.name + ", age " + std::to_string(player.age);
  drawTextCenteredHorizontally(epitaph, u8g2_font_oskool_tr, 38);
 
  std::string cause = "Cause: " + deathNames[(u8)player.death];
  drawTextCenteredHorizontally(cause, u8g2_font_oskool_tr, 50);
 
  std::string restart = "press any button to restart";
  drawTextCenteredHorizontally(restart, u8g2_font_4x6_tr, 63);
 
  if (btnLeft() || btnCenter() || btnRight()) {
    enterState(State::Title);
  }
}
 
void setup() {
  u8g2.begin();
  initButtons();
  enterState(State::Title);
}
 
void loop() {
  readButtons();
 
  u8g2.clearBuffer();
 
  switch (gameState) {
    case State::Title: updateTitle(); break;
    case State::ChooseName: updateSelectName(); break;
    case State::ChooseAge: updateSelectAge(); break;
    case State::ChooseHat: updateSelectHat(); break;
    case State::ChooseDeath: updateSelectDeath(); break;
    case State::Preview: updatePreview(); break;
    case State::Animate: updateAnimate(); break;
    case State::Result: updateResult(); break;
  }
 
  u8g2.sendBuffer();
}