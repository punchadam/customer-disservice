#pragma once

#include "types.h"

// cutscene sub-states
enum class CutscenePhase : u8 {
  WalkIn,       // character walks from left to center
  Dialogue,     // pokemon-style text box with typewriter
  ChooseDeath,  // sandbox physics + death scroller
  WhiteFlash,   // all pixels on for 1s
  DeathAnim,    // short death animation
  Done          // signals main to move to Result
};

void cutsceneBegin();
void cutsceneUpdate();
bool cutsceneIsDone();
bool cutsceneExited();  // true if user picked EXIT
Death cutsceneGetChosenDeath();