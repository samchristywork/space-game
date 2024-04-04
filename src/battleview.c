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
