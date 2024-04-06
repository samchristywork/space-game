#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>

enum Faction {
  FACTION_NEUTRAL = 0,
  FACTION_PLAYER,
  FACTION_ENEMY,
};

typedef struct Vec6 {
  long long x;
  long long y;
  long long z;
  long long a;
  long long b;
  long long c;
} Vec6;

typedef struct Vec6f {
  double x;
  double y;
  double z;
  double a;
  double b;
  double c;
} Vec6f;

typedef struct Entity {
  char *name;
  double mass;         // kg
  long long radius;    // m
  Vec6 pos;            // m
  Vec6f vel;           // m/s
  Vec6f acc;           // m/s^2
  Vec6 target;         // m
  double sensorRadius; // m
  bool moveOrder;
  bool selected;
  int faction;
} Entity;

Entity *addEntity(Entity *entities[], int *nEntities, char *name, Vec6 pos,
                  double mass, double radius, double sensorRadius);

#endif
