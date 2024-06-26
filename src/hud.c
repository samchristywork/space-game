#include <game.h>
#include <hud.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "../raygui/styles/cherry/style_cherry.h"

extern Game game;

const char *dimensions[] = {"X", "Y", "Z", "A", "B", "C"};
int speeds[] = {-2, -1, 0, 1, 2, 3, 4, 5, 6, 7};
int nSpeeds = sizeof(speeds) / sizeof(int);
const char *formations[] = {"Random", "Sphere", "Line", "Plane", "Ring"};

void drawEntitySelection(Entity *entities, int count) {
  Rectangle r = (Rectangle){10, GetScreenHeight() - 210, 600, 200};
  GuiPanel(r, "Entities (TODO)");
  static int scrollIndex = 0;
  static int focus = -1;
  static int active = -1;

  const char *names[count];
  for (int i = 0; i < count; i++) {
    names[i] = entities[i].name;
  }

  r.width = r.width / 3 - 20;

  GuiListViewEx((Rectangle){r.x + 10, r.y + 30, r.width, r.height - 40}, names,
                count, &scrollIndex, &active, &focus);
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    for (int i = 0; i < count; i++) {
      entities[i].selected = false;
    }
    entities[active].selected = true;
  }

  r.x += r.width + 20;
  r.y += 10;

  for (int i = 0; i < count; i++) {
    Entity e = entities[i];

    if (!e.selected) {
      continue;
    }

    int x = r.x + 10;
    int w = r.width;
    int stride = 20;
    GuiLabel((Rectangle){x, r.y + 1 * stride, w, 20}, "Name");
    GuiLabel((Rectangle){x, r.y + 2 * stride, w, 20}, "Mass (kg)");
    GuiLabel((Rectangle){x, r.y + 3 * stride, w, 20}, "Radius (m)");
    GuiLabel((Rectangle){x, r.y + 4 * stride, w, 20}, "Sensor Radius (m)");
    GuiLabel((Rectangle){x, r.y + 5 * stride, w, 20}, "Position (m)");
    GuiLabel((Rectangle){x, r.y + 6 * stride, w, 20}, "Velocity (m/s)");
    GuiLabel((Rectangle){x, r.y + 7 * stride, w, 20}, "Acceleration (m/s²)");
    GuiLabel((Rectangle){x, r.y + 8 * stride, w, 20}, "Target (m)");

    // TODO: Monospace
    x += r.width + 10;
    GuiLabel((Rectangle){x, r.y + 1 * stride, w, 20}, e.name);
    GuiLabel((Rectangle){x, r.y + 2 * stride, w, 20}, TextFormat("%f", e.mass));
    GuiLabel((Rectangle){x, r.y + 3 * stride, w, 20},
             TextFormat("%f", e.radius));
    GuiLabel((Rectangle){x, r.y + 4 * stride, w, 20},
             TextFormat("%d", e.sensorRadius));
    GuiLabel((Rectangle){x, r.y + 5 * stride, w, 20},
             TextFormat("%d, %d, %d", e.pos.x, e.pos.y, e.pos.z));
    GuiLabel((Rectangle){x, r.y + 6 * stride, w, 20},
             TextFormat("(%0.2f) %0.2f, %0.2f, %0.2f",
                        sqrt(e.vel.x * e.vel.x + e.vel.y * e.vel.y +
                             e.vel.z * e.vel.z),
                        e.vel.x, e.vel.y, e.vel.z));
    GuiLabel((Rectangle){x, r.y + 7 * stride, w, 20},
             TextFormat("(%0.2f) %0.2f, %0.2f, %0.2f",
                        sqrt(e.acc.x * e.acc.x + e.acc.y * e.acc.y +
                             e.acc.z * e.acc.z),
                        e.acc.x, e.acc.y, e.acc.z));
    GuiLabel((Rectangle){x, r.y + 8 * stride, w, 20},
             TextFormat("%d, %d, %d", e.target.x, e.target.y, e.target.z));
    return;
  }
}

char *printTime(double time) {
  int years = time / 31536000;
  time -= years * 31536000;
  int days = time / 86400;
  time -= days * 86400;
  int hours = time / 3600;
  time -= hours * 3600;
  int minutes = time / 60;
  time -= minutes * 60;
  int seconds = time;

  char *t = malloc(100);
  sprintf(t, "%dy%03dd%02dh%02dm%02ds", years, days, hours, minutes, seconds);
  return t;
}

void drawSpeedControl(Rectangle r) {
  static int speedIdx = 0;
  char t[100];
  sprintf(t, "Speed Control - 10e%dx", speeds[speedIdx]);
  GuiPanel(r, t);

  for (int i = 0; i < nSpeeds; i++) {
    if (i == speedIdx) {
      guiState = STATE_PRESSED;
    }

    char s[10];
    sprintf(s, "10e%dx", speeds[i]);
    int buttonWidth = (r.width - (nSpeeds + 1) * 10) / nSpeeds;
    Rectangle buttonRect = {r.x + 10 + i * (buttonWidth + 10), r.y + 30,
                            buttonWidth, 20};
    if (GuiButton(buttonRect, s)) {
      speedIdx = i;
    }
    guiState = STATE_NORMAL;
  }

  game.speed = pow(10, speeds[speedIdx]);

  char *time = printTime(game.elapsedTime);
  Rectangle timeRect = {r.x + 10, r.y + 8, r.width - 20, 20};
  GuiDrawText(time, timeRect, TEXT_ALIGN_RIGHT,
              GetColor(GuiGetStyle(LABEL, TEXT + 0)));
  free(time);
}

void drawDimensionControl(Rectangle r) {
  static int axes[3] = {0, 1, 2};
  int maxDimension = sizeof(dimensions) / sizeof(char *);

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

void drawFormationControl(Camera camera, Entity *entities, int count) {
  static int formation = 0;
  int nFormations = sizeof(formations) / sizeof(char *);
  int w = 125;
  int h = (nFormations + 2) * 25 + 20 + 13;
  Rectangle r =
      (Rectangle){10, GetScreenHeight() - 210 - h - 10 - 120 - 10, w, h};
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

  static float spacing = 1e5;
  GuiSlider((Rectangle){r.x + 10, r.y + 10 + nFormations * 25, w - 20, 20},
            "Spacing", TextFormat("%0.3e", spacing), &spacing, 1e5, 1e6);

  bool apply = false;
  if (GuiButton(
          (Rectangle){r.x + 10, r.y + 10 + (nFormations + 1) * 25, w - 20, 20},
          "Apply") ||
      IsKeyPressed(KEY_ENTER)) {
    apply = true;
  }

  int nSelected = 0;
  for (int i = 0; i < count; i++) {
    if (entities[i].selected) {
      nSelected++;
    }
  }

  if (strcmp(formations[formation], "Ring") == 0) {
    int idx = 0;
    for (int i = 0; i < count; i++) {
      if (entities[i].selected) {
        idx++;
        Vec6 target = {0, 0, 0, 0, 0, 0};
        target.x = cos(2 * PI * idx / nSelected) * spacing;
        target.z = sin(2 * PI * idx / nSelected) * spacing;
        target.x += camera.target.x * 1e5;
        target.y += camera.target.y * 1e5;
        target.z += camera.target.z * 1e5;
        Vector2 screenPos = GetWorldToScreen((Vector3){entities[i].pos.x / 1e5,
                                                       entities[i].pos.y / 1e5,
                                                       entities[i].pos.z / 1e5},
                                             camera);
        Vector2 targetScreenPos = GetWorldToScreen(
            (Vector3){target.x / 1e5, target.y / 1e5, target.z / 1e5}, camera);
        DrawLineEx(screenPos, targetScreenPos, 1, RED);

        if (apply) {
          entities[i].target = target;
          entities[i].moveOrder = true;
        }
      }
    }
  } else if (strcmp(formations[formation], "Line") == 0) {
    int idx = 0;
    for (int i = 0; i < count; i++) {
      if (entities[i].selected) {
        idx++;
        Vec6 target = {0, 0, 0, 0, 0, 0};
        target.x = idx * spacing - nSelected * spacing / 2;
        entities[i].target = target;
        entities[i].moveOrder = true;
      }
    }
  }
}

double dist3(Vector3 a, Vector3 b);

void drawEntityNameTags(Entity *entities, int count, Camera camera) {
  for (int i = 0; i < count; i++) {
    Entity e = entities[i];
    Vector3 pos = {e.pos.x / 1e5, e.pos.y / 1e5, e.pos.z / 1e5};
    Vector2 screenPos = GetWorldToScreen(pos, camera);
    screenPos.x += 10;
    screenPos.y -= 40;
    Vector2 textExtents = MeasureTextEx(GetFontDefault(), e.name, 20, 2);
    if (screenPos.x > 0 && screenPos.x < GetScreenWidth() && screenPos.y > 0 &&
        screenPos.y < GetScreenHeight()) {
      Rectangle bounds = {screenPos.x - 2, screenPos.y - 2, textExtents.x + 4,
                          textExtents.y + 4};
      DrawLineEx((Vector2){screenPos.x - 10, screenPos.y + 40},
                 (Vector2){screenPos.x - 2, screenPos.y + 20 + 1}, 1, GRAY);
      GuiDrawRectangle(bounds, 1, GetColor(GuiGetStyle(DEFAULT, LINE_COLOR)),
                       GetColor(GuiGetStyle(STATUSBAR, BASE)));
      DrawText(e.name, screenPos.x, screenPos.y, 20,
               GetColor(GuiGetStyle(DEFAULT, TEXT + (e.selected ? 1 : 0))));
    }

    if (e.moveOrder) {
      Vector2 targetScreenPos = GetWorldToScreen(
          (Vector3){e.target.x / 1e5, e.target.y / 1e5, e.target.z / 1e5},
          camera);
      DrawCircle(targetScreenPos.x, targetScreenPos.y, 5, RED);
      double dist = dist3((Vector3){e.pos.x, e.pos.y, e.pos.z},
                          (Vector3){e.target.x, e.target.y, e.target.z});
      char distStr[100];
      int numPlaces = (int)log10(dist);
      sprintf(distStr, "%0.3fe%d km", dist / pow(10, numPlaces), numPlaces);
      DrawText(distStr, screenPos.x, screenPos.y + 25, 10, RED);

      DrawText("Fuel consumption", screenPos.x, screenPos.y + 35, 10, RED);
    }
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

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  int maxDimension = sizeof(dimensions) / sizeof(char *);

  drawEntityNameTags(entities, count, camera);
  drawEntitySelection(entities, count);
  drawSpeedControl((Rectangle){sw - 650, 10, 640, 58});
  drawDimensionControl(
      (Rectangle){10, sh - 210 - 120 - 10, 40 + maxDimension * 40, 120});
}
