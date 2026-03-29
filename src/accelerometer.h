#pragma once

#include "types.h"
#include <Wire.h>

struct MPU6050 {
  u8 address = 0x68;
  bool found = false;

  float ax = 0;   // approximate acceleration in g
  float ay = 0;

  bool begin();
  void read();
};

extern MPU6050 mpu;

// physics for the preview bouncing character
void physicsUpdate(float dt);