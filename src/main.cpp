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

/*
enum Hat {
  None,
  Frog,
  Top,
  Baseball,
  Cowboy
};
struct Character {
  std::string name;
  u8 age;
  Hat hat = Hat::None;
};*/

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB14_tr);
}

u8 xpos = 0, ypos = 20;

void loop() {
  u8g2.clearBuffer();
  u8g2.drawStr(xpos, ypos, "Hello World");
  u8g2.sendBuffer();

  xpos = ++xpos % 128;
  ypos = ++ypos % 64;

  delay(1000);
}