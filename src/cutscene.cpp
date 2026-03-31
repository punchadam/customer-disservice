#include "cutscene.h"
#include "content.h"
#include "drawing.h"
#include "buttons.h"
#include "accelerometer.h"
#include <math.h>
#include "bitmaps.h"

static const unsigned char** getDeathBitmaps(Death d, int& outCount) {
  switch (d) {
    case Death::Button: outCount = button_bitmap_count; return button_bitmap_array;
    case Death::Gun:    outCount = gun_bitmap_count;    return gun_bitmap_array;
    case Death::Sword:  outCount = sword_bitmap_count;  return sword_bitmap_array;
    case Death::Kick:   outCount = kick_bitmap_count;   return kick_bitmap_array;
    case Death::Eat:    outCount = eat_bitmap_count;     return eat_bitmap_array;
    case Death::Throw:  outCount = throw_bitmap_count;   return throw_bitmap_array;
    case Death::Bale:   outCount = bale_bitmap_count;    return bale_bitmap_array;
    default:            outCount = button_bitmap_count; return button_bitmap_array;
  }
}

// cutscene state
static CutscenePhase phase;
static u32 phaseStart;
static Death chosenDeath;

// dialogue state
static u8 complaintIndex;
static std::string complaintText;
static u8 charsRevealed;
static u32 lastCharTime;
static bool dialogueDone;

// walk-in state
static float walkX;
static const float WALK_TARGET_X = 64.0f;
static const float WALK_SPEED = 60.0f;  // px/s

// death chooser state
static i8 deathIndex;
static i8 deathScrollOffset;
static bool physicsActive;
static float moveStartX, moveStartY;
static float moveTargetX, moveTargetY;
static const float MOVE_DURATION = 300.0f;  // ms

// death anim state
static u8 deathAnimPhase;

// typewriter speed (ms per character)
static const u32 TYPE_SPEED = 40;

// physics timing
static u32 lastPhysicsTime;

// did user pick EXIT in death chooser?
static bool exitPicked;

// ── death anim shared positions ──
// character on left-of-center, user (hatless) on right-of-center
static const i16 CHAR_CX = 44;
static const i16 USER_CX = 84;
static const i16 ANIM_CY = 50;  // feet y for both figures

static void enterPhase(CutscenePhase p) {
  phase = p;
  phaseStart = millis();
}

// draw a hatless stick figure (the "user" who inflicts the death)
// same proportions as drawCharacter: head at cy-CHAR_H+3, feet at cy
static void drawUser(i16 cx, i16 cy) {
  i16 headY = cy - CHAR_H + 3;
  u8g2.drawCircle(cx, headY, 3);          // head
  i16 torsoTop = headY + 3;
  i16 torsoBot = torsoTop + 5;
  u8g2.drawLine(cx, torsoTop, cx, torsoBot);              // spine
  u8g2.drawLine(cx - 4, torsoTop + 2, cx + 4, torsoTop + 2); // arms
  u8g2.drawLine(cx, torsoBot, cx - 3, cy);                // left leg
  u8g2.drawLine(cx, torsoBot, cx + 3, cy);                // right leg
}

void cutsceneBegin() {
  noiseSeed = (u16)(millis() * 7 + 0xBEEF);

  complaintIndex = noiseRand() % COMPLAINT_COUNT;
  complaintText = complaints[complaintIndex];
  charsRevealed = 0;
  lastCharTime = 0;
  dialogueDone = false;

  walkX = -10.0f;
  player.x = walkX;
  player.y = 38;  // feet just above dialogue box (starts at y=40)
  player.vx = 0;
  player.vy = 0;

  deathIndex = 0;
  deathScrollOffset = 0;
  deathAnimPhase = 0;
  physicsActive = false;
  exitPicked = false;

  enterPhase(CutscenePhase::WalkIn);
}

// dialogue text box (pokemon style, full width)
static void drawDialogueBox() {
  const i16 boxH = 24;
  const i16 boxY = 64 - boxH;
  const i16 boxX = 0;
  const i16 boxW = 128;

  u8g2.drawFrame(boxX, boxY, boxW, boxH);
  u8g2.setDrawColor(0);
  u8g2.drawBox(boxX + 1, boxY + 1, boxW - 2, boxH - 2);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_4x6_tr);
  std::string visible = complaintText.substr(0, charsRevealed);

  i16 textX = boxX + 4;
  i16 textY = boxY + 8;
  i16 lineSpacing = 8;
  std::string line;
  i8 lineNum = 0;
  for (u8 i = 0; i < visible.length(); i++) {
    if (visible[i] == '\n') {
      u8g2.drawStr(textX, textY + lineNum * lineSpacing, line.c_str());
      line.clear();
      lineNum++;
    } else {
      line += visible[i];
    }
  }
  if (line.length() > 0) {
    u8g2.drawStr(textX, textY + lineNum * lineSpacing, line.c_str());
  }

  if (dialogueDone) {
    if ((millis() / 500) % 2 == 0) {
      u8g2.drawDisc(boxX + boxW - 6, boxY + boxH - 5, 2);
    }
  }
}

// death animations
// All share: character (with hat) at (CHAR_CX, ANIM_CY)
//            user (hatless)       at (USER_CX, ANIM_CY)
// Helpers for common y-coords:
//   headY     = ANIM_CY - CHAR_H + 3   (= 39)
//   torsoTop  = headY + 3               (= 42)
//   armY      = torsoTop + 2            (= 44)
//   torsoBot  = torsoTop + 5            (= 47)

// BUTTON: user presses a button between them → character explodes
static void animateDeathButton() {
  u32 elapsed = millis() - phaseStart;

  const i16 armY = ANIM_CY - CHAR_H + 3 + 3 + 2;  // user arm-line y
  const i16 btnX = (CHAR_CX + USER_CX) / 2 - 3;    // button centered between
  const i16 btnY = ANIM_CY - 3;                      // near ground level

  if (deathAnimPhase == 0) {
    // both standing, user reaches toward button
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    u8g2.drawFrame(btnX, btnY, 7, 5);  // button outline

    float t = min(elapsed / 600.0f, 1.0f);
    // user's left arm extends toward button
    i16 handX = USER_CX - 4 - (i16)(t * (USER_CX - 4 - btnX - 4));
    i16 handY = armY + (i16)(t * (btnY + 2 - armY));
    u8g2.drawLine(USER_CX - 4, armY, handX, handY);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    // button pressed → character explodes
    drawUser(USER_CX, ANIM_CY);
    u8g2.drawBox(btnX, btnY, 7, 5);  // filled = pressed

    // arm stays on button
    u8g2.drawLine(USER_CX - 4, armY, btnX + 4, btnY + 2);

    // explosion particles from character center
    float t = min((millis() - phaseStart) / 800.0f, 1.0f);
    noiseSeed = (u16)(millis() * 3);
    i16 cx = CHAR_CX;
    i16 cy = ANIM_CY - CHAR_H / 2;
    i16 radius = (i16)(t * 22);
    for (u8 i = 0; i < 30; i++) {
      float angle = (noiseRand() % 360) * 3.14159f / 180.0f;
      float r = (noiseRand() % (radius + 1));
      i16 px = cx + (i16)(cos(angle) * r);
      i16 py = cy + (i16)(sin(angle) * r);
      u8g2.drawPixel(px, py);
    }

    if (t >= 1.0f) { enterPhase(CutscenePhase::Done); }
  }
}

// GUN: user raises arm with a small gun, fires one bullet pixel at character
static void animateDeathGun() {
  u32 elapsed = millis() - phaseStart;

  const i16 armY = ANIM_CY - CHAR_H + 3 + 3 + 2;

  if (deathAnimPhase == 0) {
    // user raises arm with gun
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    float t = min(elapsed / 400.0f, 1.0f);
    // arm goes from default left end to pointing horizontally left
    i16 handX = USER_CX - 4 - (i16)(t * 4);
    i16 handY = armY + (i16)((1.0f - t) * 5);  // starts drooping, ends level
    u8g2.drawLine(USER_CX - 4, armY, handX, handY);

    // gun appears once arm is partway up
    if (t > 0.5f) {
      u8g2.drawLine(handX, handY, handX - 5, handY);       // barrel
      u8g2.drawLine(handX - 1, handY, handX - 1, handY + 2); // grip
    }

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else if (deathAnimPhase == 1) {
    // bullet travels from gun to character
    drawUser(USER_CX, ANIM_CY);

    // arm + gun held steady
    i16 gunTipX = USER_CX - 13;
    u8g2.drawLine(USER_CX - 4, armY, USER_CX - 8, armY);
    u8g2.drawLine(USER_CX - 8, armY, gunTipX, armY);
    u8g2.drawLine(USER_CX - 9, armY, USER_CX - 9, armY + 2);

    float t = min((millis() - phaseStart) / 250.0f, 1.0f);
    i16 bulletX = gunTipX - (i16)(t * (gunTipX - CHAR_CX));
    u8g2.drawPixel(bulletX, armY);

    // character still visible until bullet hits
    if (t < 0.95f) {
      drawCharacter(CHAR_CX, ANIM_CY, true);
    }

    if (t >= 1.0f) {
      deathAnimPhase = 2;
      phaseStart = millis();
    }
  } else {
    // character falls
    drawUser(USER_CX, ANIM_CY);
    // keep gun drawn
    u8g2.drawLine(USER_CX - 4, armY, USER_CX - 8, armY);
    u8g2.drawLine(USER_CX - 8, armY, USER_CX - 13, armY);
    u8g2.drawLine(USER_CX - 9, armY, USER_CX - 9, armY + 2);

    float t = min((millis() - phaseStart) / 500.0f, 1.0f);
    i16 fallX = CHAR_CX - (i16)(t * 6);
    i16 fallY = ANIM_CY + (i16)(t * 10);
    drawCharacter(fallX, fallY, true);

    if (t >= 1.0f && (millis() - phaseStart) > 700) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// SWORD: user holds sword above head, sweeps down to slice the character in half
static void animateDeathSword() {
  u32 elapsed = millis() - phaseStart;

  const i16 headY = ANIM_CY - CHAR_H + 3;
  const i16 torsoTop = headY + 3;
  const i16 armY = torsoTop + 2;

  if (deathAnimPhase == 0) {
    // user holds sword above head, both standing
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    // arm raised up-left, hand above head
    i16 handX = USER_CX - 2;
    i16 handY = headY - 6;
    u8g2.drawLine(USER_CX - 4, armY, handX, handY);

    // sword blade pointing up from hand
    u8g2.drawLine(handX, handY, handX, handY - 10);
    // crossguard
    u8g2.drawLine(handX - 2, handY, handX + 2, handY);

    if (elapsed > 500) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else if (deathAnimPhase == 1) {
    // sword sweeps down-left toward character's midsection
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    float t = min((millis() - phaseStart) / 300.0f, 1.0f);

    // sword tip: from above user's head to character's center
    i16 startTipX = USER_CX - 2;
    i16 startTipY = headY - 16;
    i16 endTipX = CHAR_CX;
    i16 endTipY = ANIM_CY - CHAR_H / 2;

    i16 tipX = startTipX + (i16)((endTipX - startTipX) * t);
    i16 tipY = startTipY + (i16)((endTipY - startTipY) * t);

    // hand follows arc
    i16 handX = USER_CX - 4 - (i16)(t * 6);
    i16 handY = headY - 6 + (i16)(t * 14);
    u8g2.drawLine(USER_CX - 4, armY, handX, handY);
    u8g2.drawLine(handX, handY, tipX, tipY);
    u8g2.drawLine(handX - 2, handY, handX + 2, handY);

    if (t >= 1.0f) {
      deathAnimPhase = 2;
      phaseStart = millis();
    }
  } else {
    // character splits in two halves sliding apart
    drawUser(USER_CX, ANIM_CY);

    float t = min((millis() - phaseStart) / 700.0f, 1.0f);
    i16 offset = (i16)(t * 8);

    // left half slides left
    u8g2.setClipWindow(0, 0, CHAR_CX, 63);
    drawCharacter(CHAR_CX - offset, ANIM_CY, true);
    u8g2.setMaxClipWindow();

    // right half slides right (but not past user)
    u8g2.setClipWindow(CHAR_CX, 0, USER_CX - 8, 63);
    drawCharacter(CHAR_CX + offset, ANIM_CY, true);
    u8g2.setMaxClipWindow();

    if (t >= 1.0f && (millis() - phaseStart) > 900) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// KICK: user kicks the character off screen to the left
static void animateDeathKick() {
  u32 elapsed = millis() - phaseStart;

  const i16 torsoBot = ANIM_CY - CHAR_H + 3 + 3 + 5;

  if (deathAnimPhase == 0) {
    // user winds up — extends left leg toward character
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    float t = min(elapsed / 350.0f, 1.0f);

    // kicking leg extends from torsoBot toward character
    i16 footX = USER_CX - 3 - (i16)(t * (USER_CX - 3 - CHAR_CX));
    i16 footY = torsoBot + (i16)((1.0f - t) * (ANIM_CY - torsoBot));
    u8g2.drawLine(USER_CX, torsoBot, footX, footY);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    // character flies off left
    drawUser(USER_CX, ANIM_CY);

    // keep kick leg extended horizontally
    u8g2.drawLine(USER_CX, torsoBot, CHAR_CX, torsoBot);

    float t = min((millis() - phaseStart) / 500.0f, 1.0f);
    i16 flyX = CHAR_CX - (i16)(t * (CHAR_CX + 20));
    i16 flyY = (i16)(ANIM_CY - sin(t * 3.14159f) * 12);

    if (flyX > -20) {
      drawCharacter(flyX, flyY, true);
    }

    // impact sparks
    noiseSeed = (u16)(millis() * 5);
    for (u8 i = 0; i < 3; i++) {
      i16 sx = flyX + (i16)(noiseRand() % 10) - 5;
      i16 sy = flyY - CHAR_H / 2 + (i16)(noiseRand() % 8) - 4;
      u8g2.drawPixel(sx, sy);
    }

    if (t >= 1.0f && (millis() - phaseStart) > 700) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// EAT: user morphs into pacman facing left, slides into the character
static void animateDeathEat() {
  u32 elapsed = millis() - phaseStart;

  const i16 pacR = 8;
  const i16 pacCY = ANIM_CY - CHAR_H / 2;  // vertical center of figures

  if (deathAnimPhase == 0) {
    // user morphs: first half show user, second half morph to pacman
    float t = min(elapsed / 500.0f, 1.0f);

    drawCharacter(CHAR_CX, ANIM_CY, true);

    if (t < 0.5f) {
      drawUser(USER_CX, ANIM_CY);
    } else {
      // growing pacman at user position
      float st = (t - 0.5f) / 0.5f;  // 0→1 over second half
      i16 r = 3 + (i16)(st * (pacR - 3));

      u8g2.drawDisc(USER_CX, pacCY, r);
      // cut mouth wedge (facing left)
      u8g2.setDrawColor(0);
      i16 mouthW = r + 3;
      i16 mouthH = (i16)(st * 5) + 2;
      u8g2.drawTriangle(USER_CX, pacCY,
                         USER_CX - mouthW, pacCY - mouthH,
                         USER_CX - mouthW, pacCY + mouthH);
      u8g2.setDrawColor(1);
      // eye
      u8g2.drawPixel(USER_CX + 1, pacCY - r / 2 - 1);
    }

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    // pacman slides left, chomping, eats character
    float t = min((millis() - phaseStart) / 800.0f, 1.0f);

    i16 pacX = USER_CX - (i16)(t * (USER_CX - CHAR_CX + pacR + 10));

    // chomp animation
    bool mouthOpen = ((millis() / 150) % 2 == 0);
    i16 mouthH = mouthOpen ? 5 : 1;

    u8g2.drawDisc(pacX, pacCY, pacR);
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(pacX, pacCY,
                       pacX - pacR - 3, pacCY - mouthH,
                       pacX - pacR - 3, pacCY + mouthH);
    u8g2.setDrawColor(1);
    u8g2.drawPixel(pacX + 1, pacCY - pacR / 2 - 1);

    // character visible only until pacman reaches it
    if (pacX - pacR > CHAR_CX + 3) {
      drawCharacter(CHAR_CX, ANIM_CY, true);
    }

    if (t >= 1.0f && (millis() - phaseStart) > 1000) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// THROW: user grabs character with both arms, lifts, and hurls left
static void animateDeathThrow() {
  u32 elapsed = millis() - phaseStart;

  const i16 armY = ANIM_CY - CHAR_H + 3 + 3 + 2;

  if (deathAnimPhase == 0) {
    // user reaches both arms toward character
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    float t = min(elapsed / 400.0f, 1.0f);
    i16 handX = USER_CX - 4 - (i16)(t * (USER_CX - 4 - CHAR_CX));
    // two arms, slightly spread vertically
    u8g2.drawLine(USER_CX - 4, armY, handX, armY - 2);
    u8g2.drawLine(USER_CX - 4, armY, handX, armY + 2);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else if (deathAnimPhase == 1) {
    // user lifts character up
    drawUser(USER_CX, ANIM_CY);

    float t = min((millis() - phaseStart) / 300.0f, 1.0f);
    i16 liftY = ANIM_CY - (i16)(t * 12);

    // arms hold onto character
    i16 charMidY = liftY - CHAR_H / 2;
    u8g2.drawLine(USER_CX - 4, armY, CHAR_CX, charMidY - 2);
    u8g2.drawLine(USER_CX - 4, armY, CHAR_CX, charMidY + 2);

    drawCharacter(CHAR_CX, liftY, true);

    if (t >= 1.0f) {
      deathAnimPhase = 2;
      phaseStart = millis();
    }
  } else {
    // character thrown left off screen
    drawUser(USER_CX, ANIM_CY);

    float t = min((millis() - phaseStart) / 600.0f, 1.0f);
    float startY = ANIM_CY - 12;
    i16 throwX = CHAR_CX - (i16)(t * (CHAR_CX + 30));
    i16 throwY = (i16)(startY + sin(t * 3.14159f) * -15 + t * 20);

    if (throwX > -20) {
      // tumble effect
      if ((millis() / 100) % 2 == 0) {
        drawCharacter(throwX, throwY, true);
      } else {
        u8g2.drawDisc(throwX, throwY - CHAR_H / 2, 3);
      }
    }

    if (t >= 1.0f && (millis() - phaseStart) > 800) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// BALE: two horizontal lines span across the character and squish together,
// crushing everything between them to black, leaving just one flat line
static void animateDeathBale() {
  u32 elapsed = millis() - phaseStart;

  // lines only span across the character area, not reaching the user
  const i16 lineLeft = CHAR_CX - 15;
  const i16 lineRight = CHAR_CX + 15;
  const i16 charMidY = ANIM_CY - CHAR_H / 2;

  if (deathAnimPhase == 0) {
    // both standing, two horizontal lines appear above and below character
    drawCharacter(CHAR_CX, ANIM_CY, true);
    drawUser(USER_CX, ANIM_CY);

    float t = min(elapsed / 400.0f, 1.0f);

    // top line slides down from above, bottom line slides up from below
    i16 topY = charMidY - 16 + (i16)(t * 4);
    i16 botY = charMidY + 16 - (i16)(t * 4);

    u8g2.drawLine(lineLeft, topY, lineRight, topY);
    u8g2.drawLine(lineLeft, botY, lineRight, botY);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else if (deathAnimPhase == 1) {
    // lines close in, black-filling everything between them and the center
    drawUser(USER_CX, ANIM_CY);

    float t = min((millis() - phaseStart) / 700.0f, 1.0f);

    i16 topStart = charMidY - 12;
    i16 botStart = charMidY + 12;
    i16 topY = topStart + (i16)(t * (charMidY - topStart));
    i16 botY = botStart - (i16)(t * (botStart - charMidY));

    // black out region between lines (crushing the character)
    u8g2.setDrawColor(0);
    i16 crushH = botY - topY;
    if (crushH < 1) crushH = 1;
    u8g2.drawBox(lineLeft, topY, lineRight - lineLeft + 1, crushH);
    u8g2.setDrawColor(1);

    // draw character clipped to the shrinking gap
    u8g2.setClipWindow(lineLeft, topY, lineRight, botY);
    drawCharacter(CHAR_CX, ANIM_CY, true);
    u8g2.setMaxClipWindow();

    // draw the two lines
    u8g2.drawLine(lineLeft, topY, lineRight, topY);
    u8g2.drawLine(lineLeft, botY, lineRight, botY);

    if (t >= 1.0f) {
      deathAnimPhase = 2;
      phaseStart = millis();
    }
  } else {
    // single flat line remains where the character was
    drawUser(USER_CX, ANIM_CY);
    u8g2.drawLine(lineLeft, charMidY, lineRight, charMidY);

    if ((millis() - phaseStart) > 600) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

// main cutscene update

void cutsceneUpdate() {
  u32 elapsed = millis() - phaseStart;

  drawPlayerInfo();

  switch (phase) {

    case CutscenePhase::WalkIn: {
      float dt = 0.033f;
      if (walkX < WALK_TARGET_X) {
        walkX += WALK_SPEED * dt;
        if (walkX > WALK_TARGET_X) walkX = WALK_TARGET_X;
      }
      player.x = walkX;

      drawCharacter((i16)player.x, (i16)player.y, true);

      // walk animation: alternate legs
      if (walkX < WALK_TARGET_X) {
        i16 cx = (i16)player.x;
        i16 cy = (i16)player.y;
        bool step = ((millis() / 200) % 2 == 0);
        u8g2.setDrawColor(0);
        i16 torsoBot = cy - CHAR_H + 3 + 3 + 5;
        u8g2.drawLine(cx, torsoBot, cx - 3, cy);
        u8g2.drawLine(cx, torsoBot, cx + 3, cy);
        u8g2.setDrawColor(1);
        if (step) {
          u8g2.drawLine(cx, torsoBot, cx - 4, cy);
          u8g2.drawLine(cx, torsoBot, cx + 2, cy);
        } else {
          u8g2.drawLine(cx, torsoBot, cx - 2, cy);
          u8g2.drawLine(cx, torsoBot, cx + 4, cy);
        }
      }

      if (walkX >= WALK_TARGET_X) {
        enterPhase(CutscenePhase::Dialogue);
      }
      break;
    }

    case CutscenePhase::Dialogue: {
      drawCharacter((i16)player.x, (i16)player.y, true);

      u32 now = millis();
      if (!dialogueDone) {
        if (now - lastCharTime >= TYPE_SPEED) {
          lastCharTime = now;
          if (charsRevealed < complaintText.length()) {
            charsRevealed++;
            while (charsRevealed < complaintText.length() &&
                   complaintText[charsRevealed] == '\n') {
              charsRevealed++;
            }
          }
          if (charsRevealed >= complaintText.length()) {
            dialogueDone = true;
          }
        }
      }

      drawDialogueBox();

      if (btnLeft() || btnCenter() || btnRight()) {
        if (!dialogueDone) {
          charsRevealed = complaintText.length();
          dialogueDone = true;
        } else {
          // set up slide from current position to sandbox area
          moveStartX = player.x;
          moveStartY = player.y;
          moveTargetX = BOX_X + BOX_W / 2;
          moveTargetY = BOX_Y + BOX_H - 2;
          physicsActive = false;
          lastPhysicsTime = millis();
          enterPhase(CutscenePhase::ChooseDeath);
        }
      }
      break;
    }

    case CutscenePhase::ChooseDeath: {
      // slide character into position, then apply physics
      if (!physicsActive) {
        float t = min(elapsed / MOVE_DURATION, 1.0f);
        player.x = moveStartX + (moveTargetX - moveStartX) * t;
        player.y = moveStartY + (moveTargetY - moveStartY) * t;
        if (t >= 1.0f) {
          physicsActive = true;
          player.vx = 0;
          player.vy = 0;
          lastPhysicsTime = millis();
        }
      } else {
        u32 now = millis();
        float dt = (now - lastPhysicsTime) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastPhysicsTime = now;
        if (mpu.found) physicsUpdate(dt);
      }

      drawCharacter((i16)player.x, (i16)player.y);

      const i8 totalItems = (i8)Death::COUNT + 1;
      const i8 visibleRows = 4;

      if (btnLeft()) {
        deathIndex--;
        if (deathIndex < 0) deathIndex = totalItems - 1;
      }
      if (btnRight()) {
        deathIndex++;
        if (deathIndex >= totalItems) deathIndex = 0;
      }

      if (deathIndex < deathScrollOffset)
        deathScrollOffset = deathIndex;
      if (deathIndex >= deathScrollOffset + visibleRows)
        deathScrollOffset = deathIndex - visibleRows + 1;

      drawDeathScroller(deathIndex, deathScrollOffset, totalItems);

      if (btnCenter()) {
        if (deathIndex >= (i8)Death::COUNT) {
          // EXIT
          exitPicked = true;
          enterPhase(CutscenePhase::Done);
        } else {
          chosenDeath = (Death)deathIndex;
          player.death = chosenDeath;
          enterPhase(CutscenePhase::WhiteFlash);
        }
      }
      break;
    }

    case CutscenePhase::WhiteFlash: {
      const u32 FRAME_DURATION = 125;
      u8 frameIndex = elapsed / FRAME_DURATION;

      int bitmapCount;
      const unsigned char** bitmaps = getDeathBitmaps(chosenDeath, bitmapCount);

      if (frameIndex >= bitmapCount) frameIndex = bitmapCount - 1;

      u8g2.drawXBMP(0, 0, 128, 64, bitmaps[frameIndex]);

      if (elapsed >= FRAME_DURATION * (u32)bitmapCount) {
        deathAnimPhase = 0;
        player.x = CHAR_CX;
        player.y = ANIM_CY;
        enterPhase(CutscenePhase::DeathAnim);
      }
      break;
    }

    case CutscenePhase::DeathAnim: {
      switch (chosenDeath) {
        case Death::Button: animateDeathButton(); break;
        case Death::Gun:    animateDeathGun();    break;
        case Death::Sword:  animateDeathSword();  break;
        case Death::Kick:   animateDeathKick();   break;
        case Death::Eat:    animateDeathEat();     break;
        case Death::Throw:  animateDeathThrow();   break;
        case Death::Bale:   animateDeathBale();    break;
        default: enterPhase(CutscenePhase::Done); break;
      }
      break;
    }

    case CutscenePhase::Done:
      break;
  }
}

bool cutsceneIsDone() {
  return phase == CutscenePhase::Done;
}

bool cutsceneExited() {
  return phase == CutscenePhase::Done && exitPicked;
}

Death cutsceneGetChosenDeath() {
  return chosenDeath;
}