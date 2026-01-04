#ifndef WALKER_H
#define WALKER_H

#include <stdint.h>
#include "world.h"

typedef struct {
	double p_up;
	double p_down;
	double p_left;
	double p_right;
} walker_probs_t;

typedef struct {
	int x;
	int y;
	walker_probs_t probs;
} walker_t;

/* inicializuje chodca na pozícii (x,y) s danými pravdepodobnosťami */
void walker_init(walker_t *w, int x, int y, walker_probs_t probs);

/* vykoná jeden náhodný krok podľa pravdepodobností a sveta */
void walker_step(walker_t *w, const world_t *world);

#endif

