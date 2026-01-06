#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persist.h"

static int expect_word(FILE *f, const char *w)
{
	char buf[64];

	if (fscanf(f, "%63s", buf) != 1)
		return -1;
	if (strcmp(buf, w) != 0)
		return -1;
	return 0;
}

int save_simulation(const char *path,
		    const config *cfg,
		    const uint8_t *obstacles,
		    const msg_sum_cell_t *summary_cells)
{
	if (!path || !cfg || !summary_cells)
		return -1;

	FILE *f = fopen(path, "w");
	if (!f)
		return -1;

	fprintf(f, "WIDTH %d\n", cfg->world_width);
	fprintf(f, "HEIGHT %d\n", cfg->world_height);
	fprintf(f, "REPLICATIONS %u\n", (unsigned)cfg->replications);
	fprintf(f, "MAX_STEPS %u\n", (unsigned)cfg->max_steps);
	fprintf(f, "PROBS %.17g %.17g %.17g %.17g\n",
		cfg->probs.p_up, cfg->probs.p_down, cfg->probs.p_left, cfg->probs.p_right);
	fprintf(f, "WORLD_TYPE %d\n", (int)cfg->world_type);
	fprintf(f, "OBSTACLE_DENSITY %.17g\n", cfg->obstacle_density);

	int w = cfg->world_width;
	int h = cfg->world_height;

	fprintf(f, "OBSTACLES\n");
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int v = 0;
			if (cfg->world_type == WORLD_OBSTACLES && obstacles)
				v = obstacles[y * w + x] ? 1 : 0;
			fprintf(f, "%d", v);
			if (x != w - 1)
				fputc(' ', f);
		}
		fputc('\n', f);
	}

	fprintf(f, "SUMMARY\n");
	for (int i = 0; i < w * h; i++) {
		fprintf(f, "%.17g %.17g\n",
			summary_cells[i].avg_steps,
			summary_cells[i].probability);
	}

	fclose(f);
	return 0;
}

int load_simulation(const char *path,
		    config *cfg_out,
		    uint8_t **obstacles_out,
		    msg_sum_cell_t **summary_out)
{
	if (!path || !cfg_out || !obstacles_out || !summary_out)
		return -1;

	*obstacles_out = NULL;
	*summary_out = NULL;

	FILE *f = fopen(path, "r");
	if (!f)
		return -1;

	memset(cfg_out, 0, sizeof(*cfg_out));

	if (expect_word(f, "WIDTH") != 0 || fscanf(f, "%d", &cfg_out->world_width) != 1)
		goto fail;
	if (expect_word(f, "HEIGHT") != 0 || fscanf(f, "%d", &cfg_out->world_height) != 1)
		goto fail;
	if (expect_word(f, "REPLICATIONS") != 0 || fscanf(f, "%u", &cfg_out->replications) != 1)
		goto fail;
	if (expect_word(f, "MAX_STEPS") != 0 || fscanf(f, "%u", &cfg_out->max_steps) != 1)
		goto fail;
	if (expect_word(f, "PROBS") != 0 ||
	    fscanf(f, "%lf %lf %lf %lf",
		   &cfg_out->probs.p_up,
		   &cfg_out->probs.p_down,
		   &cfg_out->probs.p_left,
		   &cfg_out->probs.p_right) != 4)
		goto fail;

	int wt = 0;
	if (expect_word(f, "WORLD_TYPE") != 0 || fscanf(f, "%d", &wt) != 1)
		goto fail;
	cfg_out->world_type = (world_type_t)wt;

	if (expect_word(f, "OBSTACLE_DENSITY") != 0 || fscanf(f, "%lf", &cfg_out->obstacle_density) != 1)
		goto fail;

	int w = cfg_out->world_width;
	int h = cfg_out->world_height;
	if (w <= 0 || h <= 0)
		goto fail;

	uint8_t *obs = calloc((size_t)(w * h), 1);
	if (!obs)
		goto fail;

	if (expect_word(f, "OBSTACLES") != 0)
		goto fail_obs;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int v = 0;
			if (fscanf(f, "%d", &v) != 1)
				goto fail_obs;
			obs[y * w + x] = (v != 0) ? 1 : 0;
		}
	}

	msg_sum_cell_t *sum = calloc((size_t)(w * h), sizeof(*sum));
	if (!sum)
		goto fail_obs;

	if (expect_word(f, "SUMMARY") != 0)
		goto fail_sum;

	for (int i = 0; i < w * h; i++) {
		if (fscanf(f, "%lf %lf", &sum[i].avg_steps, &sum[i].probability) != 2)
			goto fail_sum;
	}

	fclose(f);

	*obstacles_out = obs;
	*summary_out = sum;
	return 0;

fail_sum:
	free(sum);
fail_obs:
	free(obs);
fail:
	fclose(f);
	return -1;
}

