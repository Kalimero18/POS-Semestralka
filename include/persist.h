#ifndef PERSIST_H
#define PERSIST_H

#include "config.h"
#include "protocol.h"

int save_simulation(
	const char *filename,
	const simulation_config_t *cfg,
	const msg_summary_cell_t *cells
);

int load_simulation(
	const char *filename,
	simulation_config_t *cfg,
	msg_summary_cell_t **cells_out
);

#endif

