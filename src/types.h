#pragma once

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

const std::string version = "Version 2.0.0";

// shared display instance (defined in main.cpp)
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// screen layout — sandbox box is narrower to leave room for death scroller
const i16 BOX_X = 0;
const i16 BOX_Y = 12;
const i16 BOX_W = 96;   // narrowed from 128 to leave 32px for scroller
const i16 BOX_H = 52;

// full-width box used during character creation menus
const i16 FULL_BOX_W = 128;

const u8 CHAR_W = 10;
const u8 CHAR_H = 14;

// enums

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
  Crush,
  Zap,
  Melt,
  COUNT
};

const std::string hatNames[] = {
  "None", "Frog Hat", "Top Hat", "Baseball Cap", "Cowboy Hat"
};

const std::string deathNames[] = {
  "FIRE", "CRUSH", "ZAP", "MELT"
};

// player character

struct Character {
  std::string name;
  u8 age;
  Hat hat = Hat::None;
  Death death = Death::Fire;

  // position for physics
  float x;
  float y;
  float vx;
  float vy;
};

extern Character player;

// persistent customer storage

const u8 MAX_CUSTOMERS = 4;

struct SavedCustomer {
  char name[9]; // 8 chars + null
  u8 age;
  u8 hat;       // stored as u8, cast to Hat
  bool valid;   // true if this slot is occupied
};

extern SavedCustomer savedCustomers[MAX_CUSTOMERS];
extern u8 customerCount;

void loadCustomers();
void saveCustomer(const Character& c);