#ifndef BATTLVIEW_H
#define BATTLVIEW_H

#include <entity.h>
#include <raylib.h>
#include <stars.h>

void drawBattleView(Camera *camera, Entity *entities, int numEntities,
                    Font font, Star *stars, int numStars, int *selectedStar);

#endif
