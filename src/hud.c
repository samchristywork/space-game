#include <hud.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "../raygui/styles/cherry/style_cherry.h"

void drawHud(Entity *entities, int count, Star selectedStar) {
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
