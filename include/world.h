#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

/* štruktúra sveta */
typedef struct {
    int width;
    int height;
    int x;
    int y;
    int wrap;               /* či sa svet obaluje */
    uint8_t *obstacles;     /* pole prekážok */
} world_t;

/* vytvorí svet */
world_t *w_create(int width, int height, int wrap);

/* uvoľní pamäť sveta */
void w_destroy(world_t *w);

/* skontroluje, či sú súradnice v rozsahu sveta */
int w_in_bounds(const world_t *w, int x, int y);

/* obalí súradnice na opačnú stranu sveta */
void w_wrap(const world_t *w, int *x, int *y);

/* zistí, či je na pozícii prekážka */
int w_is_obstacle(const world_t *w, int x, int y);

#endif

