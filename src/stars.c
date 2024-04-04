#include <sqlite3.h>
#include <stars.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSECS_TO_KM 3.0857e13

Star *stars = 0;
int numStars = 0;
int starCapacity = 0;

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
  if (starCapacity == 0) {
    starCapacity = 10;
    stars = malloc(starCapacity * sizeof(Star));
  }

  if (numStars >= starCapacity) {
    starCapacity *= 2;
    stars = realloc(stars, starCapacity * sizeof(Star));
    printf("Resized to %d\n", starCapacity);
  }

  stars[numStars].position =
      (Vector3){atof(argv[2]), atof(argv[3]), atof(argv[4])};
  stars[numStars].position.x *= PARSECS_TO_KM;
  stars[numStars].position.y *= PARSECS_TO_KM;
  stars[numStars].position.z *= PARSECS_TO_KM;

  stars[numStars].name = malloc(strlen(argv[0]) + 1);
  strcpy(stars[numStars].name, argv[0]);

  stars[numStars].id = malloc(strlen(argv[5]) + 1);
  strcpy(stars[numStars].id, argv[5]);

  numStars++;
  return 0;
}

Star *load_stars(int *count) {
  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("star_data.db", &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(EXIT_FAILURE);
  }

  char *sql =
      "select proper, dist, x0, y0, z0, tyc from data where cast(dist as "
      "decimal) < 70 and cast(dist as decimal) > 0.3;";

  rc = sqlite3_exec(db, sql, callback, 0, &err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to execute query. Error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(EXIT_FAILURE);
  }

  fprintf(stdout, "Operation done successfully\n");

  sqlite3_close(db);

  *count = numStars;
  return stars;
}
