#ifndef GAME_H
#define GAME_H

#include <raylib.h>

typedef struct Game {
  double speed;
  double elapsedTime;
  Font font_primary;
  Font font_secondary;
} Game;

#endif
