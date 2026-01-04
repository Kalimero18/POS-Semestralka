#include <stdio.h>

#include "persist.h"

int save_simulation(
	const char *filename,
	const simulation_config_t *cfg,
	const msg_summary_cell_t *cells
) {
	FILE *f = fopen(filename, "w");
	if (!f)
		return -1;

	fprintf(f, "WORLD %d %d\n", cfg->world_width, cfg->world_height);
	fprintf(f, "REPLICATIONS %u\n", cfg->replications);
	fprintf(f, "MAX_STEPS %u\n\n", cfg->max_steps);

	fprintf(f, "CELL y x avg_steps probability\n");

	int w = cfg->world_width;
	int h = cfg->world_height;
	int ox = w / 2;
	int oy = h / 2;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int rx = x - ox;
			int ry = oy - y;

			msg_summary_cell_t c = cells[y * w + x];

			fprintf(f, "CELL %d %d %.6f %.6f\n",
				ry, rx, c.avg_steps, c.probability);
		}
	}

	fclose(f);
	return 0;
}

