#ifndef STARS_H
#define STARS_H

#include <raylib.h>

typedef struct Star {
  Color color;
  Vector3 position;
  char *id; // Tycho Catalogue ID
  char *name;
  int temperature; // TODO: Convert to kelvin
} Star;

Star *load_stars(int *count);

#endif
