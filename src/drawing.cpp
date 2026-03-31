#include "drawing.h"

void drawPlayerInfo() {
  u8g2.setFont(u8g2_font_4x6_tr);
  std::string info = player.name + ", " + std::to_string(player.age);
  u8g2.drawStr(1, 7, info.c_str());
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

// helper: check if a bounding box is fully inside the current draw region
static bool inBox(i16 x0, i16 y0, i16 x1, i16 y1,
                  i16 clipL, i16 clipT, i16 clipR, i16 clipB) {
  return x0 >= clipL && x1 <= clipR && y0 >= clipT && y1 <= clipB;
}

void drawCharacter(i16 cx, i16 cy, bool fullScreen) {
  // determine the drawable region
  i16 clipL, clipT, clipR, clipB;
  if (fullScreen) {
    clipL = 0; clipT = 0; clipR = 127; clipB = 63;
  } else {
    clipL = BOX_X; clipT = BOX_Y;
    clipR = BOX_X + BOX_W - 1; clipB = BOX_Y + BOX_H - 1;
  }

  // Set clip window to prevent overdraw into scroller area,
  // but we also manually cull primitives to avoid U8g2 clip artifacts.
  u8g2.setClipWindow(clipL, clipT, clipR, clipB);

  i16 headY = cy - CHAR_H + 3;

  // head (circle radius 3 → bounding box cx-3..cx+3, headY-3..headY+3)
  if (inBox(cx - 3, headY - 3, cx + 3, headY + 3, clipL, clipT, clipR, clipB))
    u8g2.drawCircle(cx, headY, 3);

  // torso
  i16 torsoTop = headY + 3;
  i16 torsoBot = torsoTop + 5;
  if (inBox(cx, torsoTop, cx, torsoBot, clipL, clipT, clipR, clipB))
    u8g2.drawLine(cx, torsoTop, cx, torsoBot);

  // arms
  if (inBox(cx - 4, torsoTop + 2, cx + 4, torsoTop + 2, clipL, clipT, clipR, clipB))
    u8g2.drawLine(cx - 4, torsoTop + 2, cx + 4, torsoTop + 2);

  // left leg
  if (inBox(cx - 3, torsoBot, cx, cy, clipL, clipT, clipR, clipB))
    u8g2.drawLine(cx, torsoBot, cx - 3, cy);

  // right leg
  if (inBox(cx, torsoBot, cx + 3, cy, clipL, clipT, clipR, clipB))
    u8g2.drawLine(cx, torsoBot, cx + 3, cy);

  // hats — only draw when the whole hat is inside the region
  switch (player.hat) {
    case Hat::None:
      break;
    case Hat::Frog:
      if (inBox(cx - 5, headY - 6, cx + 5, headY - 2, clipL, clipT, clipR, clipB)) {
        u8g2.drawFilledEllipse(cx - 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
        u8g2.drawFilledEllipse(cx + 3, headY - 4, 2, 2, U8G2_DRAW_ALL);
      }
      break;
    case Hat::Top:
      if (inBox(cx - 5, headY - 8, cx + 5, headY - 3, clipL, clipT, clipR, clipB)) {
        u8g2.drawBox(cx - 3, headY - 8, 7, 5);
        u8g2.drawLine(cx - 5, headY - 3, cx + 5, headY - 3);
      }
      break;
    case Hat::Baseball:
      if (inBox(cx - 3, headY - 5, cx + 6, headY - 2, clipL, clipT, clipR, clipB)) {
        u8g2.drawFilledEllipse(cx, headY - 2, 3, 3, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
        u8g2.drawLine(cx + 3, headY - 2, cx + 6, headY - 2);
      }
      break;
    case Hat::Cowboy:
      if (inBox(cx - 6, headY - 6, cx + 6, headY - 2, clipL, clipT, clipR, clipB)) {
        u8g2.drawLine(cx - 6, headY - 2, cx + 6, headY - 2);
        u8g2.drawBox(cx - 2, headY - 6, 5, 4);
        u8g2.setDrawColor(0);
        u8g2.drawPixel(cx, headY - 6);
        u8g2.setDrawColor(1);
      }
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