#include <hud.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "../raygui/styles/cherry/style_cherry.h"

const char *dimensions[] = {"X", "Y", "Z", "A", "B", "C"};
int speeds[] = {1, 2, 4, 8, 16, 32};
int nSpeeds = sizeof(speeds) / sizeof(int);
const char *formations[] = {"Random", "Sphere", "Line", "Plane", "Ring"};

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
    Entity e = entities[i];

    if (!e.selected) {
      continue;
    }

    int x = r.x + 10;
    int w = r.width / 2 - 20;
    GuiLabel((Rectangle){x, r.y + 30, w, 20}, "Name");
    GuiLabel((Rectangle){x, r.y + 60, w, 20}, "Mass");
    GuiLabel((Rectangle){x, r.y + 90, w, 20}, "Radius");
    GuiLabel((Rectangle){x, r.y + 120, w, 20}, "Sensor Radius");
    GuiLabel((Rectangle){x, r.y + 150, w, 20}, "Position");
    GuiLabel((Rectangle){x, r.y + 180, w, 20}, "Target");

    x = r.x + r.width / 2 + 10;
    w = r.width - 20;
    GuiLabel((Rectangle){x, r.y + 30, w, 20}, e.name);
    GuiLabel((Rectangle){x, r.y + 60, w, 20}, TextFormat("%f", e.mass));
    GuiLabel((Rectangle){x, r.y + 90, w, 20}, TextFormat("%f", e.radius));
    GuiLabel((Rectangle){x, r.y + 120, w, 20},
             TextFormat("%f", e.sensorRadius));
    GuiLabel((Rectangle){x, r.y + 150, w, 20},
             TextFormat("%f, %f, %f", e.pos.x, e.pos.y, e.pos.z));
    GuiLabel((Rectangle){x, r.y + 180, w, 20},
             TextFormat("%f, %f, %f", e.target.x, e.target.y, e.target.z));
    return;
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

void drawDimensionControl() {
  static int axes[3] = {0, 1, 2};
  int maxDimension = 6;

  int w = 40 + maxDimension * 40;
  Rectangle r = (Rectangle){10, GetScreenHeight() - 220 - 120 - 10, w, 120};
  char t[100];
  sprintf(t, "Dimension Control - %s %s %s", dimensions[axes[0]],
          dimensions[axes[1]], dimensions[axes[2]]);
  GuiPanel(r, t);

  GuiLabel((Rectangle){r.x + 10, r.y + 30 + 0 * 30, 100, 20}, "X");
  GuiLabel((Rectangle){r.x + 10, r.y + 30 + 1 * 30, 100, 20}, "Y");
  GuiLabel((Rectangle){r.x + 10, r.y + 30 + 2 * 30, 100, 20}, "Z");

  for (int axis = 0; axis < 3; axis++) {
    for (int i = 0; i < maxDimension; i++) {
      if (axes[axis] == i) {
        guiState = STATE_PRESSED;
      }

      if (GuiButton(
              (Rectangle){r.x + 40 + i * 40, r.y + 30 + axis * 30, 30, 20},
              dimensions[i])) {
        axes[axis] = i;
      }

      guiState = STATE_NORMAL;
    }
  }
}

void drawFormationControl() {
  static int formation = 0;
  int nFormations = sizeof(formations) / sizeof(char *);
  int w = 125;
  int h = nFormations * 25 + 20 + 13;
  Rectangle r =
      (Rectangle){10, GetScreenHeight() - 220 - h - 10 - 120 - 10, w, h};
  char t[100];
  GuiPanel(r, "Formation Control");
  r.y += 20;
  for (int i = 0; i < nFormations; i++) {
    if (i == formation) {
      guiState = STATE_PRESSED;
    }
    if (GuiButton((Rectangle){r.x + 10, r.y + 10 + i * 25, w - 20, 20},
                  formations[i])) {
      formation = i;
    }
    guiState = STATE_NORMAL;
  }
}

void drawHud(Entity *entities, int count, Star selectedStar, Camera camera) {
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
