#include <entity.h>
#include <raylib.h>
#include <raymath.h>
#include <stars.h>
#include <stdio.h>
#include <stdlib.h>

double dist2(Vector2 a, Vector2 b) {
  return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

double dist3(Vector3 a, Vector3 b) {
  return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2) + pow(a.z - b.z, 2));
}

void drawEclipticCircle(double x, double y, double z, double radius, Color color) {
  DrawCircle3D((Vector3){x, y, z}, radius, (Vector3){1, 0, 0}, 90, color);
}

void drawEclipticCylinder(double x, double y, double z, double r, Color color) {
  double height = 0.02;
  DrawCylinder((Vector3){x, y - height / 2, z}, r, r, height, 90, color);
}

void drawReticle(double x, double y, double z) {
  drawEclipticCircle(x, y, z, 0.1f, GREEN);
  drawEclipticCircle(x, y, z, 0.05f, LIME);
  drawEclipticCircle(x, 0, z, 0.05f, GRAY);

  Vector3 source = {x, y, z};
  Vector3 dest = {x, 0, z};
  DrawLine3D(source, dest, GRAY);
}

void drawCompassDirections(Camera camera, Font font) {
  {
    Vector2 screenPos = GetWorldToScreen((Vector3){11.0f, 0.0f, 0.0f}, camera);
    DrawTextEx(font, "+X", screenPos, 20, 0, GRAY);
  }
  {
    Vector2 screenPos = GetWorldToScreen((Vector3){-11.0f, 0.0f, 0.0f}, camera);
    DrawTextEx(font, "-X", screenPos, 20, 0, GRAY);
  }
  {
    Vector2 screenPos = GetWorldToScreen((Vector3){0.0f, 0.0f, 11.0f}, camera);
    DrawTextEx(font, "+Z", screenPos, 20, 0, GRAY);
  }
  {
    Vector2 screenPos = GetWorldToScreen((Vector3){0.0f, 0.0f, -11.0f}, camera);
    DrawTextEx(font, "-Z", screenPos, 20, 0, GRAY);
  }
}

void drawGrid() {
  for (int i = 1; i <= 10; i++) {
    drawEclipticCircle(0, 0, 0, i, GRAY);
  }

  for (int i = 0; i < 10; i++) {
    double x = cos(i * 3.14159 / 5);
    double y = 0.0f;
    double z = sin(i * 3.14159 / 5);

    Vector3 source = {x, y, z};
    Vector3 dest = {x * 10, y * 10, z * 10};
    DrawLine3D(source, dest, GRAY);
  }

  for (int i = 0; i < 10; i++) {
    double x = sin(i * 3.14159 / 5);
    double y = 0.0f;
    double z = cos(i * 3.14159 / 5);

    Vector3 source = {x * 5, y * 5, z * 5};
    Vector3 dest = {x * 10, y * 10, z * 10};
    DrawLine3D(source, dest, GRAY);
  }
}

// Draws visual indicator to show the radius of a sensor for a standard target
void drawSensorRadius(Vector3 position, Camera camera, double radius) {
  Vector2 screenPos = GetWorldToScreen(position, camera);
  double distFromCamera = dist3(position, camera.position);
  double apparentSize = radius / distFromCamera;

  Color c = {0, 0, 50, 255};
  DrawCircle(screenPos.x, screenPos.y, apparentSize * 1450, c);
}

Color tempToColor(int temp) {
  unsigned char r = 0;
  unsigned char g = 255;
  unsigned char b = 0;

  if (temp < 50) {
    b = 255;
    r = 100 + temp * 5;
  } else {
    r = 255;
    b = 200 + 255 - temp;
  }

  return (Color){r, g, b, 255};
}

Vec6 vec6Add(Vec6 a, Vec6 b) {
  return (Vec6){a.x + b.x, a.y + b.y, a.z + b.z,
                a.a + b.a, a.b + b.b, a.c + b.c};
}

Vec6 vec6Scale(Vec6 a, double s) {
  return (Vec6){a.x * s, a.y * s, a.z * s, a.a * s, a.b * s, a.c * s};
}

void zoomToSelected(Camera *camera, Entity *entities, int numEntities) {
  Vec6 averagePosition = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  int count = 0;
  for (int i = 0; i < numEntities; i++) {
    Entity e = entities[i];
    if (e.selected) {
      averagePosition = vec6Add(averagePosition, e.pos);
      count++;
    }
  }
  if (count != 0) {
    averagePosition = vec6Scale(averagePosition, 1.0f / count);

    camera->position.x -= camera->target.x - averagePosition.x / 1e5;
    camera->position.y -= camera->target.y - averagePosition.y / 1e5;
    camera->position.z -= camera->target.z - averagePosition.z / 1e5;

    camera->target.x = averagePosition.x / 1e5;
    camera->target.y = averagePosition.y / 1e5;
    camera->target.z = averagePosition.z / 1e5;
  }
}

bool isBehindCamera(Vector3 position, Camera camera) {
  Vector3 forwards = Vector3Subtract(camera.target, camera.position);
  Vector3 toPosition = Vector3Subtract(position, camera.position);
  return Vector3DotProduct(forwards, toPosition) < 0;
}

void drawStars(Star *stars, int numStars, Camera *camera) {
  for (int i = 0; i < numStars; i++) {
    Star s = stars[i];
    Vector3 position = {s.position.x / 1e5, s.position.y / 1e5, s.position.z / 1e5};
    if (isBehindCamera(position, *camera)) {
      continue;
    }
    Vector2 screenPos = GetWorldToScreen(position, *camera);

    double x2 = pow(position.x - camera->position.x, 2);
    double y2 = pow(position.y - camera->position.y, 2);
    double z2 = pow(position.z - camera->position.z, 2);
    double distFromCamera = sqrt(x2 + y2 + z2);
    double radius = 6.96e5;
    double apparentSize = 10000 * radius / distFromCamera;

    if (apparentSize > 1) {
      DrawCircle(screenPos.x, screenPos.y, apparentSize, s.color);
    } else if (apparentSize > 0.7) {
      DrawPixel(screenPos.x, screenPos.y, s.color);
      DrawPixel(screenPos.x+1, screenPos.y, s.color);
      DrawPixel(screenPos.x-1, screenPos.y, s.color);
      DrawPixel(screenPos.x, screenPos.y+1, s.color);
      DrawPixel(screenPos.x, screenPos.y-1, s.color);
    } else {
      DrawPixel(screenPos.x, screenPos.y, s.color);
    }
  }
}

void drawBillboardStars(Star *stars, int numStars, Camera *camera, Texture2D star, int *selectedStar) {
  Vector2 mousePos = GetMousePosition();
  for (int i = 0; i < numStars; i++) {
    Vector3 position;
    position.x = stars[i].position.x / 1e5;
    position.y = stars[i].position.y / 1e5;
    position.z = stars[i].position.z / 1e5;

    DrawBillboard(*camera, star, position, 10000000.0f, stars[i].color);

    Vector2 screenPos = GetWorldToScreen(stars[i].position, *camera);
    bool hover = CheckCollisionPointCircle(mousePos, screenPos, 5);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover) {
      *selectedStar = i;
    }
  }
}
