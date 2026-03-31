#pragma once

#include "types.h"

void drawPlayerInfo();  // name, age in top right
void drawTextCenteredHorizontally(const std::string& txt, const uint8_t *font, i16 height);
void drawMenu(const std::string& label, const std::string& value, i8 index, i8 maxIndex);
void drawCharacter(i16 cx, i16 cy, bool fullScreen = false);
void drawFullWidthMenu(const std::string& label, const std::string& value, i8 index, i8 maxIndex);
void drawDeathScroller(i8 selectedIndex, i8 scrollOffset, i8 totalItems);