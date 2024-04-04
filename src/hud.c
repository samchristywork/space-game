#include <hud.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "../raygui/styles/cherry/style_cherry.h"

const char *dimensions[] = {"X", "Y", "Z", "A", "B", "C"};
int speeds[] = {1, 2, 4, 8, 16, 32};
int nSpeeds = sizeof(speeds) / sizeof(int);

void drawSpeedControl() {
  int w = 15 + nSpeeds * 40;
  int h = 58;
  static int speed = 0;
  Rectangle r = (Rectangle){GetScreenWidth() - w - 10, 10, w, h};
  char t[100];
  sprintf(t, "Speed Control - %dx", speeds[speed]);
  GuiPanel(r, t);
  r.y += 20;
  for (int i = 0; i < nSpeeds; i++) {
    if (i == speed) {
      guiState = STATE_PRESSED;
    }
    char s[10];
    sprintf(s, "%dx", speeds[i]);
    if (GuiButton((Rectangle){r.x + 10 + i * 40, r.y + 10, 35, 20}, s)) {
      speed = i;
    }
    guiState = STATE_NORMAL;
  }
}

  bool static initialized = false;

  if (!initialized) {
    Font f = GetFontDefault();
    f.baseSize = 20;
    GuiSetFont(f);

    GuiLoadStyleCherry();
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, TEXT_ALIGNMENT_VERTICAL, 0);
    GuiSetStyle(DEFAULT, TEXT_PADDING, 5);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 2);
    GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 20);
    GuiSetStyle(LISTVIEW, LIST_ITEMS_SPACING, 2);
  }

  // TODO: Draw the HUD
}
