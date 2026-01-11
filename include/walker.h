#ifndef WALKER_H
#define WALKER_H

#include <stdint.h>
#include "world.h"

/* pravdepodobnosti pohybu */
typedef struct {
    double p_up;
    double p_down;
    double p_left;
    double p_right;
} walker_probs_t;

typedef struct {
    int x;
    int y;
    walker_probs_t probs;   /* pravdepodobnosti pohybu */
} walker_t;

/* inicializuje chodca */
void walker_init(walker_t *w, int x, int y, walker_probs_t probs);

/* náhodný krok */
void walker_step(walker_t *w, const world_t *world);

#endif

