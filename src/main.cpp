#include "types.h"
#include "buttons.h"
#include "drawing.h"
#include "text_input.h"
#include "accelerometer.h"
#include "storage.h"

// the shared display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

// game globals
Character player;

enum class State : u8 {
  Title,
  SelectCustomer,
  ChooseName,
  ChooseAge,
  ChooseHat,
  ChooseDeath,  // kept for new-customer death preview (hat/death in creation)
  Sandbox,
  Animate,
  Result
};

State gameState = State::Title;
u32 stateEnterTime = 0;
i8 menuIndex = 0;

// title screen: 0 = New Customer, 1 = Returning Customer
i8 titleSelection = 0;

// sandbox death scroller index
i8 sandboxDeathIndex = 0;

void enterState(State s) {
  gameState = s;
  stateEnterTime = millis();
  menuIndex = 0;
}

void updateTitle() {
  u8g2.setFont(u8g2_font_oskool_tr);
  u8g2.drawStr(0, 16, "Violet's");
  drawTextCenteredHorizontally("CUSTOMER", u8g2_font_disrespectfulteenager_tu, 32);
  drawTextCenteredHorizontally("DISSERVICE", u8g2_font_disrespectfulteenager_tu, 50);

  // radio buttons at the bottom
  bool hasReturning = (customerCount > 0);

  // draw "NEW CUSTOMER" option
  u8g2.setFont(u8g2_font_4x6_tr);

  if (hasReturning) {
    // left: NEW    right: RETURNING
    i16 leftX = 4;
    i16 rightX = 68;
    i16 optY = 63;

    // radio circles
    u8g2.drawCircle(leftX + 3, optY - 3, 3);
    u8g2.drawCircle(rightX + 3, optY - 3, 3);

    // fill the selected one
    if (titleSelection == 0) {
      u8g2.drawDisc(leftX + 3, optY - 3, 1);
    } else {
      u8g2.drawDisc(rightX + 3, optY - 3, 1);
    }

    u8g2.drawStr(leftX + 9, optY, "NEW");
    u8g2.drawStr(rightX + 9, optY, "RETURNING");

    // left/right to toggle
    if (btnLeft() && titleSelection > 0) titleSelection = 0;
    if (btnRight() && titleSelection < 1) titleSelection = 1;
  } else {
    // only one option, centered
    titleSelection = 0;
    i16 optY = 63;
    i16 tw = u8g2.getStrWidth("NEW CUSTOMER");
    i16 cx = (128 - tw - 9) / 2;
    u8g2.drawCircle(cx + 3, optY - 3, 3);
    u8g2.drawDisc(cx + 3, optY - 3, 1);
    u8g2.drawStr(cx + 9, optY, "NEW CUSTOMER");
  }

  if (btnCenter()) {
    if (titleSelection == 0) {
      // new customer
      textInput.begin(8, CHARSET_ALPHA, ' ');
      enterState(State::ChooseName);
    } else {
      // returning customer
      menuIndex = 0;
      enterState(State::SelectCustomer);
    }
  }
}

// scroll offset so the visible window follows the cursor
static i8 customerScrollOffset = 0;

void updateSelectCustomer() {
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(2, 10, "RETURNING:");

  // total items = customers + EXIT
  i8 totalItems = (i8)customerCount + 1;
  const i8 visibleRows = 4;
  const i16 startY = 24;
  const i16 lineH = 11;
  const i16 listW = 124;  // leave 4px right margin for scrollbar

  // navigate
  if (btnLeft() && menuIndex > 0) menuIndex--;
  if (btnRight() && menuIndex < totalItems - 1) menuIndex++;

  // keep the scroll window following the cursor
  if (menuIndex < customerScrollOffset) {
    customerScrollOffset = menuIndex;
  }
  if (menuIndex >= customerScrollOffset + visibleRows) {
    customerScrollOffset = menuIndex - visibleRows + 1;
  }

  // draw visible rows
  u8g2.setFont(u8g2_font_oskool_tr);

  for (i8 row = 0; row < visibleRows; row++) {
    i8 itemIdx = customerScrollOffset + row;
    if (itemIdx >= totalItems) break;

    i16 y = startY + row * lineH;

    // build the label for this row
    std::string entry;
    if (itemIdx < (i8)customerCount) {
      entry = std::string(savedCustomers[itemIdx].name) + ", "
            + std::to_string(savedCustomers[itemIdx].age);
    } else {
      entry = "<< EXIT";
    }

    if (itemIdx == menuIndex) {
      u8g2.drawBox(0, y - 9, listW, lineH);
      u8g2.setDrawColor(0);
      u8g2.drawStr(6, y, entry.c_str());
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(6, y, entry.c_str());
    }
  }

  // scrollbar (only when totalItems > visibleRows)
  if (totalItems > visibleRows) {
    const i16 sbX = 126;                  // 2px-wide bar at right edge
    const i16 sbTop = startY - 9;         // top of list area
    const i16 sbH = visibleRows * lineH;  // full list height

    // track outline
    u8g2.drawLine(sbX, sbTop, sbX, sbTop + sbH - 1);

    // thumb
    i16 thumbH = max((i16)(sbH * visibleRows / totalItems), (i16)4);
    i16 maxOffset = totalItems - visibleRows;
    i16 thumbY = sbTop + (i16)((long)(sbH - thumbH) * customerScrollOffset / maxOffset);
    u8g2.drawBox(sbX - 1, thumbY, 3, thumbH);
  }

  // select
  if (btnCenter()) {
    if (menuIndex >= (i8)customerCount) {
      // EXIT selected — go back to title
      customerScrollOffset = 0;
      enterState(State::Title);
    } else {
      // load selected customer into player
      SavedCustomer& sc = savedCustomers[menuIndex];
      player.name = sc.name;
      player.age = sc.age;
      player.hat = (Hat)sc.hat;
      player.death = Death::Fire;

      customerScrollOffset = 0;
      sandboxDeathIndex = 0;
      enterState(State::Sandbox);
    }
  }
}

void updateSelectName() {
  textInput.update();

  if (textInput.done) {
    player.name = textInput.buffer;
    textInput.begin(3, CHARSET_NUMERIC, '#');
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
    // save and return to title
    saveCustomer(player);
    enterState(State::Title);
    return;
  }

  // temporarily set hat so drawCharacter renders the preview
  Hat savedHat = player.hat;
  player.hat = (Hat)menuIndex;

  drawFullWidthMenu("HAT:", hatNames[menuIndex], menuIndex, maxI);

  // draw character preview in the middle blank area (use full width for creation)
  i16 previewX = 64;  // center of full screen
  i16 previewY = BOX_Y + BOX_H - 12;

  // temporarily widen clip for the full-width preview
  u8g2.setClipWindow(0, BOX_Y, 127, BOX_Y + BOX_H);
  drawCharacter(previewX, previewY);
  u8g2.setMaxClipWindow();

  player.hat = savedHat;
}

static u32 lastPhysicsTime = 0;
static i8 sandboxScrollOffset = 0;

void updateSandbox() {
  drawCharacterBox();

  // name/age info at the top
  u8g2.setFont(u8g2_font_4x6_tr);
  std::string info = player.name + ", " + std::to_string(player.age);
  u8g2.drawStr(2, 8, info.c_str());

  // total scroller items = deaths + EXIT
  const i8 totalItems = (i8)Death::COUNT + 1;
  const i8 visibleRows = 4;

  // first frame: initialize position
  if (millis() - stateEnterTime < 20) {
    player.x = BOX_X + BOX_W / 2;
    player.y = BOX_Y + BOX_H - 2;
    player.vx = 0;
    player.vy = 0;
    lastPhysicsTime = millis();
  }

  // run physics if accelerometer is available
  u32 now = millis();
  float dt = (now - lastPhysicsTime) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  lastPhysicsTime = now;

  if (mpu.found) {
    physicsUpdate(dt);
  }

  drawCharacter((i16)player.x, (i16)player.y);

  // navigate the death scroller (wraps around)
  if (btnLeft()) {
    sandboxDeathIndex--;
    if (sandboxDeathIndex < 0) sandboxDeathIndex = totalItems - 1;
  }
  if (btnRight()) {
    sandboxDeathIndex++;
    if (sandboxDeathIndex >= totalItems) sandboxDeathIndex = 0;
  }

  // keep scroll window following the cursor
  if (sandboxDeathIndex < sandboxScrollOffset) {
    sandboxScrollOffset = sandboxDeathIndex;
  }
  if (sandboxDeathIndex >= sandboxScrollOffset + visibleRows) {
    sandboxScrollOffset = sandboxDeathIndex - visibleRows + 1;
  }

  // draw the death scroller on the right (with scroll offset + EXIT)
  drawDeathScroller(sandboxDeathIndex, sandboxScrollOffset, totalItems);

  // center button: select death and animate, or EXIT
  if (btnCenter()) {
    if (sandboxDeathIndex >= (i8)Death::COUNT) {
      // EXIT — return to title
      sandboxScrollOffset = 0;
      sandboxDeathIndex = 0;
      enterState(State::Title);
    } else {
      player.death = (Death)sandboxDeathIndex;
      sandboxScrollOffset = 0;
      enterState(State::Animate);
    }
  }
}

// simple LFSR for fast pseudo-random noise
static u16 noiseSeed = 0xACE1;
u16 noiseRand() {
  u16 bit = ((noiseSeed >> 0) ^ (noiseSeed >> 2) ^ (noiseSeed >> 3) ^ (noiseSeed >> 5)) & 1;
  noiseSeed = (noiseSeed >> 1) | (bit << 15);
  return noiseSeed;
}

// fire particles
struct Particle {
  float x, y;
  float life;
  bool active;
};

const u8 MAX_PARTICLES = 20;
Particle particles[MAX_PARTICLES];
bool particlesInit = false;

void spawnParticles() {
  for (u8 i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x = player.x + (float)((i16)(noiseRand() % 16) - 8);
    particles[i].y = player.y - (float)(noiseRand() % CHAR_H);
    particles[i].life = 0.5f + (noiseRand() % 100) / 50.0f;
    particles[i].active = true;
  }
  particlesInit = true;
}

void animateFire() {
  u32 elapsed = millis() - stateEnterTime;

  drawCharacterBox();

  if (elapsed < 1000) {
    drawCharacter((i16)player.x, (i16)player.y);

    u8g2.setClipWindow(BOX_X + 1, BOX_Y + 1, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
    noiseSeed = (u16)(millis() * 7 + 0xBEEF);
    u16 density = 80 + (elapsed * 200) / 1000;
    for (u16 i = 0; i < density; i++) {
      i16 px = BOX_X + 1 + (noiseRand() % (BOX_W - 2));
      i16 py = BOX_Y + 1 + (noiseRand() % (BOX_H - 2));
      u8g2.setDrawColor((noiseRand() & 1) ? 1 : 0);
      u8g2.drawPixel(px, py);
    }
    u8g2.setDrawColor(1);
    u8g2.setMaxClipWindow();
  } else {
    if (!particlesInit) spawnParticles();

    float dt = 0.033f;
    if (mpu.found) mpu.read();

    float driftX = mpu.found ? -mpu.ax * 60.0f : 0.0f;
    float driftY = mpu.found ? -mpu.ay * 60.0f : -40.0f;

    u8g2.setClipWindow(BOX_X + 1, BOX_Y + 1, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);

    bool anyAlive = false;
    for (u8 i = 0; i < MAX_PARTICLES; i++) {
      if (!particles[i].active) continue;

      particles[i].x += driftX * dt + (float)((i16)(noiseRand() % 5) - 2) * 0.5f;
      particles[i].y += driftY * dt + (float)((i16)(noiseRand() % 3) - 1) * 0.3f;
      particles[i].life -= dt;

      if (particles[i].life <= 0) {
        particles[i].active = false;
        continue;
      }

      anyAlive = true;
      u8g2.drawPixel((i16)particles[i].x, (i16)particles[i].y);
      if (particles[i].life > 0.8f) {
        u8g2.drawPixel((i16)particles[i].x + 1, (i16)particles[i].y);
      }
    }

    u8g2.setMaxClipWindow();

    if (!anyAlive || elapsed > 4000) {
      particlesInit = false;
      enterState(State::Result);
    }
  }
}

// crush animation state
struct CrushState {
  i16 boxX, boxY, boxW, boxH;
  u8 phase;
};

CrushState crush;

void animateCrush() {
  u32 elapsed = millis() - stateEnterTime;

  if (elapsed < 20) {
    crush.boxX = BOX_X;
    crush.boxY = BOX_Y;
    crush.boxW = BOX_W;
    crush.boxH = BOX_H;
    crush.phase = 0;
  }

  if (crush.phase == 0) {
    float t = min(elapsed / 1500.0f, 1.0f);
    float ease = t * t;

    crush.boxH = max((i16)(BOX_H * (1.0f - ease)), (i16)1);
    crush.boxY = BOX_Y + (BOX_H - crush.boxH) / 2;

    if (crush.boxH > 2) {
      u8g2.drawFrame(crush.boxX, crush.boxY, crush.boxW, crush.boxH);
      i16 charY = crush.boxY + crush.boxH - 1;
      u8g2.setClipWindow(crush.boxX + 1, crush.boxY + 1,
                         crush.boxX + crush.boxW - 1, crush.boxY + crush.boxH - 1);
      drawCharacter((i16)player.x, charY);
      u8g2.setMaxClipWindow();
    } else {
      i16 lineY = BOX_Y + BOX_H / 2;
      u8g2.drawLine(crush.boxX, lineY, crush.boxX + crush.boxW - 1, lineY);
    }

    if (t >= 1.0f) {
      crush.phase = 1;
      stateEnterTime = millis();
    }
  } else if (crush.phase == 1) {
    float t = min(elapsed / 1000.0f, 1.0f);
    float ease = t * t;

    i16 lineY = BOX_Y + BOX_H / 2;
    i16 centerX = BOX_X + BOX_W / 2;

    i16 halfW = (i16)((BOX_W / 2) * (1.0f - ease));
    if (halfW < 1) halfW = 0;

    if (halfW == 0) {
      u8g2.drawPixel(centerX, lineY);
    } else {
      u8g2.drawLine(centerX - halfW, lineY, centerX + halfW, lineY);
    }

    if (t >= 1.0f) {
      crush.phase = 2;
      stateEnterTime = millis();
    }
  } else {
    i16 lineY = BOX_Y + BOX_H / 2;
    i16 centerX = BOX_X + BOX_W / 2;
    u8g2.drawPixel(centerX, lineY);

    if (elapsed > 500) {
      enterState(State::Result);
    }
  }
}

void animateZap() {
  u32 elapsed = millis() - stateEnterTime;

  drawCharacterBox();

  bool visible = (elapsed / 80) % 2 == 0;
  if (visible) {
    drawCharacter((i16)player.x, (i16)player.y);
  }

  u8g2.setClipWindow(BOX_X + 1, BOX_Y + 1, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
  noiseSeed = (u16)(millis() * 13 + 0xCAFE);

  u8 numBolts = 2 + (elapsed / 500);
  if (numBolts > 5) numBolts = 5;
  for (u8 b = 0; b < numBolts; b++) {
    i16 bx = (i16)player.x + (i16)(noiseRand() % 20) - 10;
    i16 by = BOX_Y + 2;
    for (u8 seg = 0; seg < 6; seg++) {
      i16 nx = bx + (i16)(noiseRand() % 9) - 4;
      i16 ny = by + (noiseRand() % 8) + 2;
      u8g2.drawLine(bx, by, nx, ny);
      bx = nx;
      by = ny;
      if (by > BOX_Y + BOX_H - 2) break;
    }
  }
  u8g2.setMaxClipWindow();

  if (elapsed > 2000) {
    float fade = (elapsed - 2000) / 1000.0f;
    if (fade >= 1.0f) {
      enterState(State::Result);
      return;
    }

    u8g2.setClipWindow(BOX_X + 1, BOX_Y + 1, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
    u8 count = (u8)((1.0f - fade) * 15);
    for (u8 i = 0; i < count; i++) {
      i16 px = (i16)player.x + (i16)(noiseRand() % 20) - 10;
      i16 py = (i16)player.y - (i16)(noiseRand() % CHAR_H) - 2;
      u8g2.drawPixel(px, py);
    }
    u8g2.setMaxClipWindow();
  }
}

void animateMelt() {
  u32 elapsed = millis() - stateEnterTime;

  drawCharacterBox();

  u8g2.setClipWindow(BOX_X + 1, BOX_Y + 1, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);

  float t = elapsed / 3000.0f;
  if (t > 1.0f) t = 1.0f;

  i16 floorY = BOX_Y + BOX_H - 2;

  float sinkAmount = t * (float)(CHAR_H + 4);
  i16 charY = (i16)(player.y + sinkAmount);
  if (charY > floorY) charY = floorY;

  if (t < 0.7f) {
    drawCharacter((i16)player.x, charY);
  }

  noiseSeed = 0xD00D;
  for (u8 i = 0; i < 8; i++) {
    i16 dripX = (i16)player.x + (i16)(noiseRand() % 12) - 6;
    float dripDelay = (noiseRand() % 100) / 100.0f * 0.5f;
    float dripT = t - 0.2f - dripDelay;
    if (dripT < 0) continue;

    i16 dripY = charY + (i16)(dripT * 40.0f);
    if (dripY >= floorY) {
      u8g2.drawPixel(dripX, floorY);
      if (t > 0.5f) u8g2.drawPixel(dripX + 1, floorY);
    } else {
      u8g2.drawPixel(dripX, dripY);
      u8g2.drawPixel(dripX, dripY + 1);
    }
  }

  if (t > 0.3f) {
    float puddleT = (t - 0.3f) / 0.7f;
    i16 puddleW = (i16)(puddleT * 16);
    i16 cx = (i16)player.x;
    u8g2.drawLine(cx - puddleW, floorY, cx + puddleW, floorY);
    if (puddleW > 4) {
      u8g2.drawLine(cx - puddleW + 2, floorY - 1, cx + puddleW - 2, floorY - 1);
    }
  }

  u8g2.setMaxClipWindow();

  if (elapsed > 3500) {
    enterState(State::Result);
  }
}

void updateAnimate() {
  switch (player.death) {
    case Death::Fire:  animateFire();  break;
    case Death::Crush: animateCrush(); break;
    case Death::Zap:   animateZap();   break;
    case Death::Melt:  animateMelt();  break;
    default: enterState(State::Result); break;
  }
}

void updateResult() {
  drawTextCenteredHorizontally("R.I.P.", u8g2_font_disrespectfulteenager_tu, 20);

  std::string epitaph = player.name + ", age " + std::to_string(player.age);
  drawTextCenteredHorizontally(epitaph, u8g2_font_oskool_tr, 38);

  std::string cause = "Cause: " + deathNames[(u8)player.death];
  drawTextCenteredHorizontally(cause, u8g2_font_oskool_tr, 50);

  drawTextCenteredHorizontally("press any button", u8g2_font_4x6_tr, 63);

  if (btnLeft() || btnCenter() || btnRight()) {
    // return to sandbox so they can pick another death
    sandboxDeathIndex = (i8)player.death;
    enterState(State::Sandbox);
  }
}

void setup() {
  Wire.begin();
  u8g2.begin();
  initButtons();
  mpu.begin();
  loadCustomers();
  enterState(State::Title);
}

void loop() {
  readButtons();

  u8g2.clearBuffer();

  switch (gameState) {
    case State::Title:          updateTitle();          break;
    case State::SelectCustomer: updateSelectCustomer(); break;
    case State::ChooseName:     updateSelectName();     break;
    case State::ChooseAge:      updateSelectAge();      break;
    case State::ChooseHat:      updateSelectHat();      break;
    case State::ChooseDeath:    break;
    case State::Sandbox:        updateSandbox();        break;
    case State::Animate:        updateAnimate();        break;
    case State::Result:         updateResult();         break;
  }

  u8g2.sendBuffer();
}