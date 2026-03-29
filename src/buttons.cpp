#include "buttons.h"

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
    buttons[i] = { pins[i], true, true, false, 0 };
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

bool btnLeft()   { return buttons[0].pressed; }
bool btnCenter() { return buttons[1].pressed; }
bool btnRight()  { return buttons[2].pressed; }