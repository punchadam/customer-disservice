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

const char* version = "Version 1.0.0";

/*
enum Hat {
  None,
  Frog,
  Top,
  Baseball,
  Cowboy
};
enum Death {
  None,
  Fire,
  Fall
};
struct Character {
  std::string name;
  u8 age;
  Hat hat = Hat::None;
};*/

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const u8 PIN_LEFT = D0;
const u8 PIN_CENTER = D1;
const u8 PIN_RIGHT = D2;

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
    pinMode(pins[i], INPUT);
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

void startupScreen() {
  u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_jinxedwizards_tr);
  u8g2.setFont(u8g2_font_oskool_tr);
  u8g2.drawStr(0, 5 + 11, "Violet's");
  // u8g2.setFont(u8g2_font_sticker100complete_tr);
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(3, 5 + 11 + 16, "CUSTOMER");
  u8g2.drawStr(3, 5 + 11 + 16 + 2 + 16, "DISSERVICE");
  //u8g2.setFont(u8g2_font_stylishcharm_tr);
  //u8g2.drawStr(0, 64, version);
  u8g2.sendBuffer();
  delay(2000);
}

void setup() {
  u8g2.begin();
  startupScreen();
}

void loop() {
  delay(1000);
}