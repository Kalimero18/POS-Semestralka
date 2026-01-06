#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdint.h>
#include "world.h"
#include "walker.h"

typedef struct {
	uint8_t hit;
	uint32_t steps;
} rep_cell_res_t;

typedef struct {
	int width;
	int height;
	rep_cell_res_t *cells;
} rep_res_t;

/* vykoná jednu replikáciu nad celým svetom */
rep_res_t *rep_run(
	const world_t *world,
	const walker_probs_t *probs,
	uint32_t max_steps
);

void rep_destroy(rep_res_t *res);

/* získa výsledok pre políčko (x,y) */
rep_cell_res_t rep_get(
	const rep_res_t *res,
	int x,
	int y
);

#endif

