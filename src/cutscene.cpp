#include "cutscene.h"
#include "content.h"
#include "drawing.h"
#include "buttons.h"
#include "accelerometer.h"
#include <math.h>

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

static void enterPhase(CutscenePhase p) {
  phase = p;
  phaseStart = millis();
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

static void animateDeathButton() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    drawCharacter((i16)player.x, (i16)player.y, true);

    i16 btnX = (i16)player.x - 5;
    i16 btnY = (i16)player.y + 4;
    u8g2.drawFrame(btnX, btnY, 11, 7);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(btnX + 2, btnY + 6, "OK");

    if (elapsed > 800) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    noiseSeed = (u16)(millis() * 3);
    float t = elapsed / 800.0f;
    if (t > 1.0f) { enterPhase(CutscenePhase::Done); return; }

    i16 cx = (i16)player.x;
    i16 cy = (i16)player.y - CHAR_H / 2;
    i16 radius = (i16)(t * 20);
    for (u8 i = 0; i < 30; i++) {
      float angle = (noiseRand() % 360) * 3.14159f / 180.0f;
      float r = (noiseRand() % (radius + 1));
      i16 px = cx + (i16)(cos(angle) * r);
      i16 py = cy + (i16)(sin(angle) * r);
      u8g2.drawPixel(px, py);
    }
  }
}

static void animateDeathGun() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    drawCharacter((i16)player.x, (i16)player.y, true);

    float gunProgress = min(elapsed / 400.0f, 1.0f);
    i16 gunX = 128 - (i16)(gunProgress * 60);
    i16 gunY = (i16)player.y - CHAR_H / 2;
    u8g2.drawBox(gunX, gunY - 1, 8, 3);
    u8g2.drawLine(gunX, gunY, gunX - 4, gunY);

    if (elapsed > 600) {
      u8g2.drawLine(gunX - 4, gunY, (i16)player.x + 4, gunY);
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    float t = min((millis() - phaseStart) / 600.0f, 1.0f);
    i16 fallY = (i16)(player.y + t * 10);
    drawCharacter((i16)player.x + (i16)(t * 6), fallY, true);

    if (t >= 1.0f && (millis() - phaseStart) > 800) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

static void animateDeathSword() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    drawCharacter((i16)player.x, (i16)player.y, true);

    float t = min(elapsed / 500.0f, 1.0f);
    float angle = -0.8f + t * 1.6f;
    i16 hiltX = (i16)player.x + 12;
    i16 hiltY = (i16)player.y - CHAR_H - 5;
    i16 tipX = hiltX + (i16)(sin(angle) * 18);
    i16 tipY = hiltY + (i16)(cos(angle) * 18);
    u8g2.drawLine(hiltX, hiltY, tipX, tipY);
    u8g2.drawLine(hiltX - 2, hiltY + 1, hiltX + 2, hiltY + 1);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    float t = min((millis() - phaseStart) / 700.0f, 1.0f);
    i16 offset = (i16)(t * 8);

    u8g2.setClipWindow(0, 0, (i16)player.x, 63);
    drawCharacter((i16)player.x - offset, (i16)player.y, true);
    u8g2.setMaxClipWindow();

    u8g2.setClipWindow((i16)player.x, 0, 127, 63);
    drawCharacter((i16)player.x + offset, (i16)player.y, true);
    u8g2.setMaxClipWindow();

    if (t >= 1.0f && (millis() - phaseStart) > 900) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

static void animateDeathKick() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    drawCharacter((i16)player.x, (i16)player.y, true);

    float t = min(elapsed / 400.0f, 1.0f);
    i16 bootX = 128 - (i16)(t * (128 - player.x + 10));
    i16 bootY = (i16)player.y - CHAR_H / 2 + 2;
    u8g2.drawBox(bootX, bootY - 2, 8, 5);
    u8g2.drawBox(bootX - 3, bootY + 2, 11, 2);
    u8g2.drawLine(bootX + 8, bootY, min((i16)127, (i16)(bootX + 20)), bootY - 5);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    float t = min((millis() - phaseStart) / 500.0f, 1.0f);
    i16 flyX = (i16)(player.x - t * (player.x + 20));
    i16 flyY = (i16)(player.y - sin(t * 3.14159f) * 15);
    if (flyX > -20) {
      drawCharacter(flyX, flyY, true);
    }
    for (u8 i = 0; i < 3; i++) {
      i16 sx = flyX + (i16)(noiseRand() % 12) - 6;
      i16 sy = flyY - CHAR_H + (i16)(noiseRand() % 8) - 4;
      u8g2.drawPixel(sx, sy);
    }

    if (t >= 1.0f && (millis() - phaseStart) > 700) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

static void animateDeathEat() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    float t = min(elapsed / 600.0f, 1.0f);
    i16 mouthOpen = (i16)(t * 30);

    drawCharacter((i16)player.x, (i16)player.y, true);

    i16 jawY = 63 - mouthOpen;
    u8g2.drawLine(0, jawY, 127, jawY);
    for (i16 tx = 4; tx < 128; tx += 12) {
      u8g2.drawTriangle(tx, jawY, tx + 4, jawY, tx + 2, jawY - 3);
    }
    i16 topY = jawY - 40;
    if (topY < 0) topY = 0;
    u8g2.drawLine(0, topY, 127, topY);
    for (i16 tx = 4; tx < 128; tx += 12) {
      u8g2.drawTriangle(tx, topY, tx + 4, topY, tx + 2, topY + 3);
    }

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    float t = min((millis() - phaseStart) / 400.0f, 1.0f);
    i16 midY = 32;
    i16 gap = (i16)((1.0f - t) * 20);

    u8g2.drawLine(0, midY - gap, 127, midY - gap);
    u8g2.drawLine(0, midY + gap, 127, midY + gap);
    for (i16 tx = 4; tx < 128; tx += 12) {
      u8g2.drawTriangle(tx, midY - gap, tx + 4, midY - gap, tx + 2, midY - gap + 3);
      u8g2.drawTriangle(tx, midY + gap, tx + 4, midY + gap, tx + 2, midY + gap - 3);
    }

    if (gap <= 0) {
      u8g2.drawLine(0, midY, 127, midY);
    }

    if (t >= 1.0f && (millis() - phaseStart) > 600) {
      enterPhase(CutscenePhase::Done);
    }
  }
}

static void animateDeathThrow() {
  u32 elapsed = millis() - phaseStart;

  if (deathAnimPhase == 0) {
    float t = min(elapsed / 500.0f, 1.0f);
    i16 liftY = (i16)(player.y - t * 20);
    drawCharacter((i16)player.x, liftY, true);

    i16 handY = liftY - CHAR_H - 3;
    u8g2.drawLine((i16)player.x - 3, handY, (i16)player.x + 3, handY);
    u8g2.drawLine((i16)player.x - 3, handY, (i16)player.x - 3, handY + 3);
    u8g2.drawLine((i16)player.x + 3, handY, (i16)player.x + 3, handY + 3);

    if (t >= 1.0f) {
      deathAnimPhase = 1;
      phaseStart = millis();
    }
  } else {
    float t = min((millis() - phaseStart) / 600.0f, 1.0f);
    float startY = player.y - 20;
    i16 throwX = (i16)(player.x + t * 100);
    i16 throwY = (i16)(startY + sin(t * 3.14159f) * -20 + t * 30);

    if (throwX < 140) {
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
      u8g2.drawBox(0, 0, 128, 64);

      if (elapsed >= 1000) {
        deathAnimPhase = 0;
        player.x = 64;
        player.y = 50;
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
        case Death::Eat:    animateDeathEat();    break;
        case Death::Throw:  animateDeathThrow();  break;
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