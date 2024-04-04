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
