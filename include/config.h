#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "walker.h"
#include "protocol.h"

typedef struct {
	int world_width;
	int world_height;
	uint32_t replications;
	uint32_t max_steps;
	walker_probs_t probs;
	sim_mode_t mode;
	char output_file[256];
} simulation_config_t;

#endif

