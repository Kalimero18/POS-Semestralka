#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

typedef struct {
	int width;
	int height;
	int x;
	int y;
	int wrap;
	uint8_t *obstacles;
} world_t;

/* vytvorí svet so zadanými rozmermi */
world_t *w_create(int width, int height, int wrap);

/* uvoľní pamäť sveta */
void w_destroy(world_t *w);

/* preverí, či sú súradnice vo svete */
int w_in_bounds(const world_t *w, int x, int y);

void w_wrap(const world_t *w, int *x, int *y);

/* zistí, či je políčko prekážka */
int w_is_obstacle(const world_t *w, int x, int y);

#endif

