#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdint.h>
#include "world.h"
#include "walker.h"

/* výsledok pre jedno políčko */
typedef struct {
    uint8_t hit;        /* či sa dosiahol cieľ */
    uint32_t steps;
} rep_cell_res_t;

typedef struct {
    int width;
    int height;
    rep_cell_res_t *cells;
} rep_res_t;

rep_res_t *rep_run(
    const world_t *world,
    const walker_probs_t *probs,
    uint32_t max_steps
);

/* uvoľní pamäť*/
void rep_destroy(rep_res_t *res);

rep_cell_res_t rep_get(
    const rep_res_t *res,
    int x,
    int y
);

#endif

