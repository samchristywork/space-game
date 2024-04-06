#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>

enum Faction {
  FACTION_NEUTRAL = 0,
  FACTION_PLAYER,
  FACTION_ENEMY,
};

typedef struct Vec6 {
  double x;
  double y;
  double z;
  double a;
  double b;
  double c;
} Vec6;

typedef struct Entity {
  const char *name;
  double mass;         // kg
  double radius;       // km
  Vec6 pos;            // km
  Vec6 vel;            // km/s
  Vec6 acc;            // km/s^2
  Vec6 target;         // km
  double sensorRadius; // km
  bool moveOrder;
  bool selected;
  int faction;
} Entity;

Entity *addEntity(Entity *entities[], int *nEntities, char *name, Vec6 pos,
                  double mass, double radius, double sensorRadius);

#endif
