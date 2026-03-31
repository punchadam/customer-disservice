#include "types.h"
#include "buttons.h"
#include "drawing.h"
#include "text_input.h"
#include "accelerometer.h"
#include "storage.h"
#include "cutscene.h"

// the shared display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

// game globals
Character player;
bool deathCounted = false;

// shared LFSR noise
u16 noiseSeed = 0xACE1;
u16 noiseRand() {
  u16 bit = ((noiseSeed >> 0) ^ (noiseSeed >> 2) ^ (noiseSeed >> 3) ^ (noiseSeed >> 5)) & 1;
  noiseSeed = (noiseSeed >> 1) | (bit << 15);
  return noiseSeed;
}

enum class State : u8 {
  Title,
  SelectCustomer,
  ChooseName,
  ChooseAge,
  ChooseHat,
  Cutscene,
  Result
};

State gameState = State::Title;
u32 stateEnterTime = 0;
i8 menuIndex = 0;

// title screen: 0 = New Customer, 1 = Returning Customer
i8 titleSelection = 0;

void enterState(State s) {
  gameState = s;
  stateEnterTime = millis();
  menuIndex = 0;
}

// draw a string character-by-character, each with an individual Y offset
static void drawStringPerChar(const char* text, const uint8_t* font, i16 baseY, float sweepX) {
  u8g2.setFont(font);
  i16 totalW = u8g2.getStrWidth(text);
  i16 startX = (128 - totalW) / 2;

  const float BUMP_RADIUS = 20.0f;  // how far from sweep center chars are affected
  const float BUMP_HEIGHT = 3.0f;   // max pixels to bump up

  char buf[2] = {0, 0};
  i16 curX = startX;
  for (u8 i = 0; text[i] != '\0'; i++) {
    buf[0] = text[i];
    i16 charW = u8g2.getStrWidth(buf);
    float charCenter = curX + charW / 2.0f;

    // how close is this character to the sweep line?
    float dist = charCenter - sweepX;
    if (dist < 0) dist = -dist;

    i16 yOff = 0;
    if (dist < BUMP_RADIUS) {
      float t = 1.0f - (dist / BUMP_RADIUS);  // 1 at center, 0 at edge
      yOff = (i16)(t * t * BUMP_HEIGHT);       // quadratic falloff
    }

    u8g2.drawStr(curX, baseY - yOff, buf);
    curX += charW;
  }
}

// shimmer: letter bump + diagonal black lines sweeping across the logo
static void drawTitleShimmer() {
  const u32 PERIOD   = 6000;
  const u32 SWEEP_MS = 1200;

  u32 t = millis() % PERIOD;
  bool sweeping = (t < SWEEP_MS);

  // sweep position (-10 to 138 so it enters/exits fully)
  float sweepX = sweeping ? (-10.0f + (t / (float)SWEEP_MS) * 148.0f) : -999.0f;

  // draw logo text with per-character bump (always draw, bump only during sweep)
  drawStringPerChar("CUSTOMER", u8g2_font_disrespectfulteenager_tu, 32, sweepX);
  drawStringPerChar("DISSERVICE", u8g2_font_disrespectfulteenager_tu, 50, sweepX);

  // diagonal black lines (only during sweep)
  if (sweeping) {
    u8g2.setDrawColor(0);
    const i16 logoTop = 22;
    const i16 logoBot = 51;
    for (i16 y = logoTop; y <= logoBot; y++) {
      i16 x1 = (i16)(sweepX + (y - logoTop) * 0.4f);
      i16 x2 = x1 + 4;
      if (x1 >= 0 && x1 <= 127) u8g2.drawPixel(x1, y);
      if (x2 >= 0 && x2 <= 127) u8g2.drawPixel(x2, y);
    }
    u8g2.setDrawColor(1);
  }
}

void updateTitle() {
  u8g2.setFont(u8g2_font_oskool_tr);
  u8g2.drawStr(0, 16, "Violet's");

  // death counter top-right
  u8g2.setFont(u8g2_font_4x6_tr);
  std::string dc = "Deaths:" + std::to_string(deathCount);
  i16 dcW = u8g2.getStrWidth(dc.c_str());
  u8g2.drawStr(128 - dcW, 6, dc.c_str());

  // logo text + shimmer
  drawTitleShimmer();

  bool hasReturning = (customerCount > 0);

  u8g2.setFont(u8g2_font_4x6_tr);

  if (hasReturning) {
    i16 leftX = 4;
    i16 rightX = 68;
    i16 optY = 63;

    u8g2.drawCircle(leftX + 3, optY - 3, 3);
    u8g2.drawCircle(rightX + 3, optY - 3, 3);

    if (titleSelection == 0) {
      u8g2.drawDisc(leftX + 3, optY - 3, 1);
    } else {
      u8g2.drawDisc(rightX + 3, optY - 3, 1);
    }

    u8g2.drawStr(leftX + 9, optY, "NEW");
    u8g2.drawStr(rightX + 9, optY, "RETURNING");

    if (btnLeft() && titleSelection > 0) titleSelection = 0;
    if (btnRight() && titleSelection < 1) titleSelection = 1;
  } else {
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
      textInput.begin(8, CHARSET_ALPHA, ' ');
      enterState(State::ChooseName);
    } else {
      menuIndex = 0;
      enterState(State::SelectCustomer);
    }
  }
}

static i8 customerScrollOffset = 0;

void updateSelectCustomer() {
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(2, 10, "RETURNING:");

  i8 totalItems = (i8)customerCount + 1;
  const i8 visibleRows = 4;
  const i16 startY = 24;
  const i16 lineH = 11;
  const i16 listW = 124;

  if (btnLeft() && menuIndex > 0) menuIndex--;
  if (btnRight() && menuIndex < totalItems - 1) menuIndex++;

  if (menuIndex < customerScrollOffset)
    customerScrollOffset = menuIndex;
  if (menuIndex >= customerScrollOffset + visibleRows)
    customerScrollOffset = menuIndex - visibleRows + 1;

  u8g2.setFont(u8g2_font_oskool_tr);

  for (i8 row = 0; row < visibleRows; row++) {
    i8 itemIdx = customerScrollOffset + row;
    if (itemIdx >= totalItems) break;

    i16 y = startY + row * lineH;

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

  if (totalItems > visibleRows) {
    const i16 sbX = 126;
    const i16 sbTop = startY - 9;
    const i16 sbH = visibleRows * lineH;

    u8g2.drawLine(sbX, sbTop, sbX, sbTop + sbH - 1);

    i16 thumbH = max((i16)(sbH * visibleRows / totalItems), (i16)4);
    i16 maxOffset = totalItems - visibleRows;
    i16 thumbY = sbTop + (i16)((long)(sbH - thumbH) * customerScrollOffset / maxOffset);
    u8g2.drawBox(sbX - 1, thumbY, 3, thumbH);
  }

  if (btnCenter()) {
    if (menuIndex >= (i8)customerCount) {
      customerScrollOffset = 0;
      enterState(State::Title);
    } else {
      SavedCustomer& sc = savedCustomers[menuIndex];
      player.name = sc.name;
      player.age = sc.age;
      player.hat = (Hat)sc.hat;
      player.death = Death::Button;

      customerScrollOffset = 0;
      cutsceneBegin();
      enterState(State::Cutscene);
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
    saveCustomer(player);
    cutsceneBegin();
    enterState(State::Cutscene);
    return;
  }

  // temporarily set hat so drawCharacter renders the preview
  Hat savedHat = player.hat;
  player.hat = (Hat)menuIndex;

  drawFullWidthMenu("HAT:", hatNames[menuIndex], menuIndex, maxI);

  i16 previewX = 64;
  i16 previewY = BOX_Y + BOX_H - 12;

  u8g2.setClipWindow(0, BOX_Y, 127, BOX_Y + BOX_H);
  drawCharacter(previewX, previewY);
  u8g2.setMaxClipWindow();

  player.hat = savedHat;
}

void updateCutscene() {
  cutsceneUpdate();

  if (cutsceneIsDone()) {
    if (cutsceneExited()) {
      enterState(State::Title);
    } else {
      player.death = cutsceneGetChosenDeath();
      enterState(State::Result);
    }
  }
}

void updateResult() {
  if (!deathCounted) {
    incrementDeathCount();
    deathCounted = true;
  }
  drawTextCenteredHorizontally("R.I.P.", u8g2_font_disrespectfulteenager_tu, 20);

  std::string epitaph = player.name + ", age " + std::to_string(player.age);
  drawTextCenteredHorizontally(epitaph, u8g2_font_oskool_tr, 38);

  std::string cause = "Cause: " + deathNames[(u8)player.death];
  drawTextCenteredHorizontally(cause, u8g2_font_oskool_tr, 50);

  drawTextCenteredHorizontally("press any button", u8g2_font_4x6_tr, 63);

  if (btnLeft() || btnCenter() || btnRight()) {
    deathCounted = false;
    cutsceneBegin();
    enterState(State::Cutscene);
  }
}

void setup() {
  Wire.begin();
  u8g2.begin();
  initButtons();
  mpu.begin();
  loadCustomers();
  loadDeathCount();
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
    case State::Cutscene:       updateCutscene();       break;
    case State::Result:         updateResult();         break;
  }

  u8g2.sendBuffer();
}