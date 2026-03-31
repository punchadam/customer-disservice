#include "drawing.h"

void drawPlayerInfo() {
  u8g2.setFont(u8g2_font_4x6_tr);
  std::string info = player.name + ", " + std::to_string(player.age);
  i16 tw = u8g2.getStrWidth(info.c_str());
  u8g2.drawStr(127 - tw, 7, info.c_str());
}

void drawTextCenteredHorizontally(const std::string& txt, const uint8_t *font, i16 height) {
  u8g2.setFont(font);
  i16 tw = u8g2.getStrWidth(txt.c_str());
  u8g2.drawStr((128 - tw) / 2, height, txt.c_str());
}

// original menu uses full 128px width (for hat selection etc.)
void drawFullWidthMenu(const std::string& label, const std::string& value, i8 index, i8 maxIndex) {
  u8g2.setFont(u8g2_font_disrespectfulteenager_tu);
  u8g2.drawStr(2, 10, label.c_str());
  u8g2.setFont(u8g2_font_oskool_tr);
  drawTextCenteredHorizontally(value, u8g2_font_oskool_tr, 63);

  if (index > 0) {
    u8g2.setFont(u8g2_font_oskool_tr);
    u8g2.drawStr(4, 63, "<");
  }
  if (index < maxIndex - 1) {
    u8g2.setFont(u8g2_font_oskool_tr);
    u8g2.drawStr(118, 63, ">");
  }
}

// kept for compatibility but uses the narrower BOX_W
void drawMenu(const std::string& label, const std::string& value, i8 index, i8 maxIndex) {
  drawFullWidthMenu(label, value, index, maxIndex);
}

void drawCharacter(i16 cx, i16 cy, bool fullScreen) {
  if (fullScreen) {
    u8g2.setClipWindow(0, 0, 127, 63);
  } else {
    u8g2.setClipWindow(BOX_X, BOX_Y, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
  }

  i16 headY = cy - CHAR_H + 3;
  u8g2.drawCircle(cx, headY, 3);

  i16 torsoTop = headY + 3;
  i16 torsoBot = torsoTop + 5;
  u8g2.drawLine(cx, torsoTop, cx, torsoBot);

  u8g2.drawLine(cx - 4, torsoTop + 2, cx + 4, torsoTop + 2);

  u8g2.drawLine(cx, torsoBot, cx - 3, cy);
  u8g2.drawLine(cx, torsoBot, cx + 3, cy);

  switch (player.hat) {
    case Hat::None:
      break;
    case Hat::Frog:
      u8g2.drawFilledEllipse(cx - 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
      u8g2.drawFilledEllipse(cx + 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
      break;
    case Hat::Top:
      u8g2.drawBox(cx - 3, headY - 8, 7, 5);
      u8g2.drawLine(cx - 5, headY - 3, cx + 5, headY - 3);
      break;
    case Hat::Baseball:
      u8g2.drawFilledEllipse(cx, headY - 2, 3, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
      u8g2.drawLine(cx + 3, headY - 2, cx + 6, headY - 2);
      break;
    case Hat::Cowboy:
      u8g2.drawLine(cx - 6, headY - 2, cx + 6, headY - 2);
      u8g2.drawBox(cx - 2, headY - 6, 5, 4);
      u8g2.setDrawColor(0);
      u8g2.drawPixel(cx, headY - 6);
      u8g2.setDrawColor(1);
      break;
    default:
      break;
  }

  // restore full clip window
  u8g2.setMaxClipWindow();
}

// vertical death scroller drawn to the right of the character box
void drawDeathScroller(i8 selectedIndex, i8 scrollOffset, i8 totalItems) {
  const i16 scrollX = BOX_X + BOX_W;
  const i16 scrollW = 128 - scrollX - 3;
  const i8 visibleRows = 4;
  const i16 itemH = BOX_H / visibleRows;

  // clip drawing to the scroller area
  u8g2.setClipWindow(scrollX - 1, BOX_Y, 127, BOX_Y + BOX_H - 1);

  for (i8 row = 0; row < visibleRows; row++) {
    i8 itemIdx = scrollOffset + row;
    if (itemIdx >= totalItems) break;

    i16 iy = BOX_Y + row * itemH;

    // build label
    std::string label;
    if (itemIdx < (i8)Death::COUNT) {
      label = deathNames[itemIdx];
    } else {
      label = "EXIT";
    }

    if (itemIdx == selectedIndex) {
      u8g2.drawBox(scrollX, iy, scrollW, itemH);
      u8g2.setDrawColor(0);
    }

    u8g2.setFont(u8g2_font_4x6_tr);
    i16 tw = u8g2.getStrWidth(label.c_str());
    i16 tx = scrollX + (scrollW - tw) / 2;
    i16 ty = iy + itemH / 2 + 3;
    u8g2.drawStr(tx, ty, label.c_str());

    u8g2.setDrawColor(1);
  }

  u8g2.setMaxClipWindow();

  // scrollbar (only when totalItems > visibleRows)
  if (totalItems > visibleRows) {
    const i16 sbX = 126;
    const i16 sbTop = BOX_Y;
    const i16 sbH = BOX_H;

    i16 thumbH = max((i16)(sbH * visibleRows / totalItems), (i16)4);
    i16 maxOff = totalItems - visibleRows;
    i16 thumbY = sbTop + (i16)((long)(sbH - thumbH) * scrollOffset / maxOff);
    u8g2.drawLine(sbX, thumbY, sbX, thumbY + thumbH - 1);
  }
}