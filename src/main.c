#include <battleview.h>
#include <camera.h>
#include <entity.h>
#include <hud.h>
#include <levels.h>
#include <raylib.h>
#include <raymath.h>
#include <stars.h>

Entity *entities;
int nEntities = 0;

int main() {
  int numStars = 0;
  Star *stars = load_stars(&numStars);

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1920, 1080, "TODO");
  SetTargetFPS(60);
  SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE | FLAG_FULLSCREEN_MODE);

  Font font = LoadFont("./assets/fonts/LiterationMonoNerdFont-Regular.ttf");

  Camera camera = {0};
  camera.position = (Vector3){10.0f, 10.0f, 10.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  int cameraMode = CAMERA_FREE;

  SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);

  int messageState = 0;
  int opacity = 0;
  int opacityTarget = 255;
  int fadeRate = 40;

  level(0, &entities, &nEntities);

  while (!WindowShouldClose()) {
    BeginDrawing();

    handleCamera(&camera, cameraMode);

    if (IsKeyPressed(KEY_M)) {
      for (int i = 0; i < nEntities; i++) {
        Entity *e = &entities[i];
        if (e->selected) {
          e->target.x = camera.target.x * 1e5;
          e->target.y = camera.target.y * 1e5;
          e->target.z = camera.target.z * 1e5;
          e->moveOrder = true;
        }
      }
    }

    if (IsKeyPressed(KEY_KP_0) || IsKeyPressed('0')) {
      camera.position = Vector3Subtract(camera.position, camera.target);
      camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    }

    if (IsKeyPressed(KEY_A) && IsKeyDown(KEY_LEFT_CONTROL)) {
      for (int i = 0; i < nEntities; i++) {
        entities[i].selected = true;
      }
    }

    ClearBackground((Color){0, 0, 0, 255});

    static int selectedStar = 0;
    drawBattleView(&camera, entities, nEntities, font, stars, numStars,
                   &selectedStar);

    static float fuzziness = 1.0f;
    if (fuzziness > 0) {
      for (int i = 0; i < GetScreenWidth(); i += 10) {
        for (int j = 0; j < GetScreenHeight(); j += 10) {
          if (rand() % 100 < 100 * fuzziness) {
            DrawRectangle(i, j, 10, 10, (Color){0, 0, 0, 255});
            char s[2] = "X";
            s[0] = rand() % 26 + 'A';
            DrawText(s, i, j, 15, (Color){rand() % 255, 0, 0, 100});
          }
        }
      }
      fuzziness -= 0.05;
    }

    drawHud(entities, nEntities, stars[selectedStar], camera);

    DrawFPS(10, 10);
    EndDrawing();
  }
}
