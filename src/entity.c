#include <entity.h>
#include <stdlib.h>
#include <string.h>

Entity *addEntity(Entity *entities[], int *nEntities, char *name, Vec6 pos,
                  double mass, double radius, double sensorRadius) {
  *entities = (Entity *)realloc(*entities, sizeof(Entity) * (*nEntities + 1));
  bzero(*entities + *nEntities, sizeof(Entity));
  (*entities)[*nEntities].name = malloc(strlen(name) + 1);
  strcpy((*entities)[*nEntities].name, name);
  (*entities)[*nEntities].mass = mass;
  (*entities)[*nEntities].radius = radius;
  (*entities)[*nEntities].pos = pos;
  (*entities)[*nEntities].sensorRadius = sensorRadius;
  (*nEntities)++;

  return &(*entities)[*nEntities - 1];
}
