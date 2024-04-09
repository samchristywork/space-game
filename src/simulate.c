#include <entity.h>
#include <game.h>
#include <raylib.h>
#include <raymath.h>

extern Game game;

void simulate(Camera *camera, Entity *entities, int count, double dt) {
  game.elapsedTime += dt;

  for (int i = 0; i < count; i++) {
    Entity *e = &entities[i];

    e->vel.x += e->acc.x * dt;
    e->vel.y += e->acc.y * dt;
    e->vel.z += e->acc.z * dt;

    e->pos.x += e->vel.x * dt;
    e->pos.y += e->vel.y * dt;
    e->pos.z += e->vel.z * dt;

    if (e->moveOrder) {
      Vector3 position = (Vector3){e->pos.x, e->pos.y, e->pos.z};
      Vector3 velocity = (Vector3){e->vel.x, e->vel.y, e->vel.z};
      Vector3 target = {e->target.x, e->target.y, e->target.z};

      Vector3 heading = Vector3Normalize(velocity);
      Vector3 desired = Vector3Normalize(Vector3Subtract(target, position));

      float dot = Vector3DotProduct(heading, desired);
      Vector3 along = Vector3Scale(Vector3Normalize(desired), dot);
      Vector3 corrective = Vector3Normalize(Vector3Subtract(along, heading));

      Vector3 acc;
      if (Vector3Length(Vector3Subtract(heading, desired)) > 0.1) {
        acc = Vector3Add(Vector3Scale(corrective, 10), desired);
      } else {
        acc = Vector3Add(Vector3Scale(desired, 10), corrective);
      }

      float distanceToTarget = Vector3Length(Vector3Subtract(target, position));
      float speed = Vector3Length(velocity);
      double acceleration = 9.81; // 1g

      float timeToStop = speed / acceleration;
      float distanceToStop =
          speed * timeToStop - 0.5 * acceleration * timeToStop * timeToStop;

      if (distanceToStop < distanceToTarget) {
        acc = Vector3Scale(Vector3Normalize(acc), acceleration);
      } else {
        Vector3 antiVel = Vector3Scale(Vector3Normalize(velocity), -1);
        acc = Vector3Scale(Vector3Normalize(antiVel), 10);
      }

      e->acc.x = acc.x;
      e->acc.y = acc.y;
      e->acc.z = acc.z;
    } else {
      e->acc.x = 0;
      e->acc.y = 0;
      e->acc.z = 0;
    }
  }
}
