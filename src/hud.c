#include <hud.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "../raygui/styles/cherry/style_cherry.h"

const char *dimensions[] = {"X", "Y", "Z", "A", "B", "C"};
int speeds[] = {1, 2, 4, 8, 16, 32};
int nSpeeds = sizeof(speeds) / sizeof(int);

void drawEntitySelection(Entity *entities, int count) {
  Rectangle r = (Rectangle){10, GetScreenHeight() - 220, 600, 210};
  GuiPanel(r, "Entities (TODO)");
  static int scrollIndex = 0;
  static int focus = -1;
  static int active = -1;

  const char *names[count];
  for (int i = 0; i < count; i++) {
    names[i] = entities[i].name;
  }

  GuiListViewEx(
      (Rectangle){r.x + 10, r.y + 30, r.width / 3 - 20, r.height - 40}, names,
      count, &scrollIndex, &active, &focus);
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    for (int i = 0; i < count; i++) {
      entities[i].selected = false;
    }
    entities[active].selected = true;
  }

  r.x += r.width / 3;
  r.width = r.width / 3;

  for (int i = 0; i < count; i++) {
    if (entities[i].selected) {
      GuiLabel((Rectangle){r.x + 10, r.y + 30, r.width / 2 - 20, 20}, "Name");
      GuiLabel((Rectangle){r.x + 10, r.y + 60, r.width / 2 - 20, 20}, "Mass");
      GuiLabel((Rectangle){r.x + 10, r.y + 90, r.width / 2 - 20, 20}, "Radius");
      GuiLabel((Rectangle){r.x + 10, r.y + 120, r.width / 2 - 20, 20},
               "Sensor Radius");
      GuiLabel((Rectangle){r.x + 10, r.y + 150, r.width / 2 - 20, 20},
               "Position");
      GuiLabel((Rectangle){r.x + 10, r.y + 180, r.width / 2 - 20, 20},
               "Target");

      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 30, r.width - 20, 20},
               entities[i].name);
      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 60, r.width - 20, 20},
               TextFormat("%f", entities[i].mass));
      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 90, r.width - 20, 20},
               TextFormat("%f", entities[i].radius));
      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 120, r.width - 20, 20},
               TextFormat("%f", entities[i].sensorRadius));
      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 150, r.width - 20, 20},
               TextFormat("%f, %f, %f", entities[i].pos.x, entities[i].pos.y,
                          entities[i].pos.z));
      GuiLabel((Rectangle){r.x + r.width / 2 + 10, r.y + 180, r.width - 20, 20},
               TextFormat("%f, %f, %f", entities[i].target.x,
                          entities[i].target.y, entities[i].target.z));
      return;
    }
  }
}

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
