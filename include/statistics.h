#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include "world.h"
#include "replication.h"

typedef struct {
	uint32_t hits;
	uint32_t steps_sum;
} statistics_cell_t;

typedef struct {
	int width;
	int height;
	uint32_t replications;
	statistics_cell_t *cells;
} statistics_t;

statistics_t *statistics_create(const world_t *world, uint32_t replications);

/* pridá výsledok jednej replikácie */
void statistics_add_replication(
	statistics_t *stats,
	const replication_result_t *rep
);

double statistics_avg_steps(
	const statistics_t *stats,
	int x,
	int y
);

/* vypočíta pravdepodobnosť dosiahnutia [0,0] */
double statistics_probability(
	const statistics_t *stats,
	int x,
	int y
);

/* uvoľní štatistiku */
void statistics_destroy(statistics_t *stats);

#endif

