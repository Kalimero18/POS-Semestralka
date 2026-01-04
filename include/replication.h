#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdint.h>
#include "world.h"
#include "walker.h"

typedef struct {
	uint8_t hit;
	uint32_t steps;
} replication_cell_result_t;

typedef struct {
	int width;
	int height;
	replication_cell_result_t *cells;
} replication_result_t;

/* vykoná jednu replikáciu nad celým svetom */
replication_result_t *replication_run(
	const world_t *world,
	const walker_probs_t *probs,
	uint32_t max_steps
);

void replication_destroy(replication_result_t *res);

/* získa výsledok pre políčko (x,y) */
replication_cell_result_t replication_get(
	const replication_result_t *res,
	int x,
	int y
);

#endif

