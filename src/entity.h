#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>

enum Faction {
  FACTION_NEUTRAL = 0,
  FACTION_PLAYER,
  FACTION_ENEMY,
};

typedef struct Vec6 {
  float x;
  float y;
  float z;
  float a;
  float b;
  float c;
} Vec6;

typedef struct Entity {
  const char *name;
  float mass;         // kg
  float radius;       // km
  Vec6 pos;           // km
  Vec6 vel;           // km/s
  Vec6 acc;           // km/s^2
  Vec6 target;        // km
  float sensorRadius; // km
  bool moveOrder;
  bool selected;
  int faction;
} Entity;

#endif
