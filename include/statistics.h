#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include "world.h"
#include "replication.h"

typedef struct {
	uint32_t hits;
	uint32_t steps_sum;
} stat_cell_t;

typedef struct {
	int width;
	int height;
	uint32_t replications;
	stat_cell_t *cells;
} stat_t;

stat_t *stat_create(const world_t *world, uint32_t replications);

/* pridá výsledok jednej replikácie */
void stat_add_rep(
	stat_t *stats,
	const rep_res_t *rep
);

double stat_avg_steps(
	const stat_t *stats,
	int x,
	int y
);

/* vypočíta pravdepodobnosť dosiahnutia [0,0] */
double stat_probability(
	const stat_t *stats,
	int x,
	int y
);

/* uvoľní štatistiku */
void stat_destroy(stat_t *stats);

#endif

