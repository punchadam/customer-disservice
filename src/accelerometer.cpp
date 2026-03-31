#include "accelerometer.h"

MPU6050 mpu;

// MPU6050 register addresses
const u8 REG_PWR_MGMT_1 = 0x6B;
const u8 REG_ACCEL_XOUT_H = 0x3B;
const u8 REG_WHO_AM_I = 0x75;

bool MPU6050::begin() {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0) { found = true; }

  if (!found) return false;

  // wake it up (clear sleep bit in PWR_MGMT_1)
  Wire.beginTransmission(address);
  Wire.write(REG_PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission();

  delay(10);
  return true;
}

void MPU6050::read() {
  if (!found) return;

  Wire.beginTransmission(address);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(address, (u8)6);

  if (Wire.available() < 6) return;

  i16 rawAx = (Wire.read() << 8) | Wire.read();
  i16 rawAy = (Wire.read() << 8) | Wire.read();
  // skip Z (2 bytes)
  Wire.read(); Wire.read();

  // default sensitivity is +/- 2g -> 16384 LSB/g
  ay = rawAx / 16384.0f;
  ax = rawAy / 16384.0f;
}

// physics for bouncing character in the box (uses the narrower sandbox box)

const float PHYS_LEFT   = BOX_X + 5;
const float PHYS_RIGHT  = BOX_X + BOX_W - 5;
const float PHYS_TOP    = BOX_Y + CHAR_H + 1;
const float PHYS_BOTTOM = BOX_Y + BOX_H - 1;

const float GRAVITY_SCALE = 1200.0f;  // pixels/s² per g
const float BOUNCE  = 0.8f;           // coefficient of restitution
const float DAMPING = 0.98f;          // velocity damping per frame

void physicsUpdate(float dt) {
  if (!mpu.found) return;

  mpu.read();

  player.vx += mpu.ax * GRAVITY_SCALE * dt;
  player.vy += mpu.ay * GRAVITY_SCALE * dt;

  player.vx *= DAMPING;
  player.vy *= DAMPING;

  player.x += player.vx * dt;
  player.y += player.vy * dt;

  // bounce off walls
  if (player.x < PHYS_LEFT) {
    player.x = PHYS_LEFT;
    player.vx = -player.vx * BOUNCE;
  }
  if (player.x > PHYS_RIGHT) {
    player.x = PHYS_RIGHT;
    player.vx = -player.vx * BOUNCE;
  }
  if (player.y < PHYS_TOP) {
    player.y = PHYS_TOP;
    player.vy = -player.vy * BOUNCE;
  }
  if (player.y > PHYS_BOTTOM) {
    player.y = PHYS_BOTTOM;
    player.vy = -player.vy * BOUNCE;
  }
}