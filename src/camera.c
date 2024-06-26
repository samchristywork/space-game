#include <math.h>
#include <raylib.h>
#include <raymath.h>

void moveRight(Camera *c, double amount) {
  double distance = Vector3Distance(c->position, c->target);
  amount *= distance / 5;

  Vector3 forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(c->up, forward));

  c->position = Vector3Add(c->position, Vector3Scale(right, amount));
  c->target = Vector3Add(c->target, Vector3Scale(right, amount));
}

void moveUp(Camera *c, double amount) {
  double distance = Vector3Distance(c->position, c->target);
  amount *= distance / 5;

  Vector3 vUp = {0.0f, 1.0f, 0.0f};
  Vector3 vRight = Vector3Normalize(Vector3CrossProduct(c->up, c->position));

  c->position = Vector3Add(c->position, Vector3Scale(vUp, amount));
  c->target = Vector3Add(c->target, Vector3Scale(vUp, amount));
}

void moveForward(Camera *c, double amount) {
  double distance = Vector3Distance(c->position, c->target);
  amount *= distance / 5;

  Vector3 dir = Vector3Subtract(c->target, c->position);
  dir.y = 0;
  Vector3 forward = Vector3Normalize(dir);

  c->position = Vector3Add(c->position, Vector3Scale(forward, amount));
  c->target = Vector3Add(c->target, Vector3Scale(forward, amount));
}

void orbitRight(Camera *c, double amount) {
  Matrix matRotation = MatrixRotate(c->up, amount);

  c->position = Vector3Subtract(c->position, c->target);
  c->position = Vector3Transform(c->position, matRotation);
  c->position = Vector3Add(c->position, c->target);
}

void orbitUp(Camera *c, double amount) {
  Vector3 forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
  Vector3 vRight = Vector3Normalize(Vector3CrossProduct(c->up, forward));

  double angle = acos(Vector3DotProduct(forward, c->up));

  if (angle + amount > M_PI) {
    return;
  }

  if (angle + amount < 0) {
    return;
  }

  Matrix matRotation = MatrixRotate(vRight, amount);

  c->position = Vector3Subtract(c->position, c->target);
  c->position = Vector3Transform(c->position, matRotation);
  c->position = Vector3Add(c->position, c->target);
}

void setCameraDistanceFromTarget(Camera *c, double distance) {
  Vector3 direction = Vector3Normalize(Vector3Subtract(c->target, c->position));

  c->position = Vector3Subtract(c->target, Vector3Scale(direction, distance));
}

void zoomTowardTarget(Camera *c, double amount) {
  double distance = Vector3Distance(c->position, c->target);
  double newDistance = distance + amount * distance / 5;

  if (newDistance < 0.01) {
    newDistance = 0.01;
  }

  setCameraDistanceFromTarget(c, newDistance);
}

void handleCamera(Camera *c, int mode) {
  Vector2 mousePositionDelta = GetMouseDelta();

  if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    moveRight(c, mousePositionDelta.x * 0.01);
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      moveUp(c, mousePositionDelta.y * 0.01);
    } else {
      moveForward(c, mousePositionDelta.y * 0.01);
    }
  }

  if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
    orbitRight(c, -mousePositionDelta.x * 0.005);
    orbitUp(c, mousePositionDelta.y * 0.005);
  }

  zoomTowardTarget(c, -GetMouseWheelMove());
}
