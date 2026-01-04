#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

typedef struct {
	int width;
	int height;
	int origin_x;
	int origin_y;
	int wrap;
	uint8_t *obstacles;
} world_t;

/* vytvorí svet so zadanými rozmermi */
world_t *world_create(int width, int height, int wrap);

/* uvoľní pamäť sveta */
void world_destroy(world_t *w);

/* preverí, či sú súradnice vo svete */
int world_in_bounds(const world_t *w, int x, int y);

void world_apply_wrap(const world_t *w, int *x, int *y);

/* zistí, či je políčko prekážka */
int world_is_obstacle(const world_t *w, int x, int y);

#endif

