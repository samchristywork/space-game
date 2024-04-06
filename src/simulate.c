#include <entity.h>
#include <raylib.h>
#include <raymath.h>

void simulate(Camera *camera, Entity *entities, int count) {
  for (int i = 0; i < count; i++) {
    Entity *e = &entities[i];

    e->vel.x += e->acc.x;
    e->vel.y += e->acc.y;
    e->vel.z += e->acc.z;

    e->pos.x += e->vel.x;
    e->pos.y += e->vel.y;
    e->pos.z += e->vel.z;

    if (e->moveOrder) {
      Vector3 position = (Vector3){e->pos.x, e->pos.y, e->pos.z};
      Vector3 velocity = (Vector3){e->vel.x, e->vel.y, e->vel.z};
      Vector3 target = {e->target.x, e->target.y, e->target.z};

      Vector3 heading = Vector3Normalize(velocity);
      Vector3 desired = Vector3Normalize(Vector3Subtract(target, position));
      float dot = Vector3DotProduct(heading, desired);
      Vector3 along = Vector3Scale(Vector3Normalize(desired), dot);
      Vector3 corrective = Vector3Normalize(Vector3Subtract(along, heading));
      Vector3 acc = Vector3Add(corrective, desired);

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
