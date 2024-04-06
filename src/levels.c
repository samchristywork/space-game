#include <entity.h>
#include <stdlib.h>

void sampleLevel(Entity *entities[], int *nEntities) {
  addEntity(entities, nEntities, "Earth", (Vec6){4e5, 0, 0, 0, 0, 0}, 5.972e24,
            6.371e3, 5e4);

  Entity *e = addEntity(entities, nEntities, "Moon", (Vec6){0, 4e5, 0, 0, 0, 0},
                        7.342e22, 1.737e3, 5e3);

  float v = 9e2;
  e->vel.x = 0.5 * v;
  e->vel.y = -v;
  e->vel.z = 0.5 * v;

  addEntity(entities, nEntities, "Mars", (Vec6){0, 0, 4e5, 0, 0, 0}, 6.39e23,
            3.3895e3, 5e4);

  addEntity(entities, nEntities, "Jupiter", (Vec6){-4e5, 0, 0, 0, 0, 0},
            1.898e27, 6.9911e4, 5e4);

  addEntity(entities, nEntities, "Saturn", (Vec6){0, -4e5, 0, 0, 0, 0},
            5.683e26, 5.8232e4, 5e4);

  addEntity(entities, nEntities, "Uranus", (Vec6){0, 0, -4e5, 0, 0, 0},
            8.681e25, 2.5362e4, 5e4);
}

void level(int level, Entity **entities, int *nEntities) {
  *entities = (Entity *)malloc(0);
  *nEntities = 0;

  switch (level) {
  case 0:
    sampleLevel(entities, nEntities);
    break;
  default:
    sampleLevel(entities, nEntities);
    break;
  }
}
