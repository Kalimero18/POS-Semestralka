#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>

#include "config.h"
#include "protocol.h"

int save_simulation(const char *path,
		    const config *cfg,
		    const uint8_t *obstacles,
		    const msg_sum_cell_t *summary_cells);

int load_simulation(const char *path,
		    config *cfg_out,
		    uint8_t **obstacles_out,
		    msg_sum_cell_t **summary_out);

#endif

