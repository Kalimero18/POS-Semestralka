#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persist.h"

/* očakáva konkrétne kľúčové slovo v súbore - Odporúčené AI */
static int expect_word(FILE *file, const char *word)
{
    char buffer[64];

    if (fscanf(file, "%63s", buffer) != 1)
        return -1;

    if (strcmp(buffer, word) != 0)
        return -1;

    return 0;
}

/* uloží konfiguráciu, prekážky a výsledky do súboru */
int save_simulation(const char *path,
                    const config *cfg,
                    const uint8_t *obstacles,
                    const msg_sum_cell_t *summary_cells)
{
    if (!path || !cfg || !summary_cells)
        return -1;

    FILE *file = fopen(path, "w");
    if (!file)
        return -1;

    fprintf(file, "WIDTH %d\n", cfg->world_width);
    fprintf(file, "HEIGHT %d\n", cfg->world_height);
    fprintf(file, "REPLICATIONS %u\n", (unsigned)cfg->replications);
    fprintf(file, "MAX_STEPS %u\n", (unsigned)cfg->max_steps);

    fprintf(file, "PROBS %.17g %.17g %.17g %.17g\n",
            cfg->probs.p_up,
            cfg->probs.p_down,
            cfg->probs.p_left,
            cfg->probs.p_right);

    fprintf(file, "WORLD_TYPE %d\n", (int)cfg->world_type);
    fprintf(file, "OBSTACLE_DENSITY %.17g\n", cfg->obstacle_density);

    int width = cfg->world_width;
    int height = cfg->world_height;

    fprintf(file, "OBSTACLES\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int value = 0;

            if (cfg->world_type == WORLD_OBSTACLES && obstacles)
                value = obstacles[y * width + x] ? 1 : 0;

            fprintf(file, "%d", value);

            if (x != width - 1)
                fputc(' ', file);
        }
        fputc('\n', file);
    }

    fprintf(file, "SUMMARY\n");
    for (int i = 0; i < width * height; i++) {
        fprintf(file, "%.17g %.17g\n",
                summary_cells[i].avg_steps,
                summary_cells[i].probability);
    }

    fclose(file);
    return 0;
}

/* načíta simuláciu zo súboru */
int load_simulation(const char *path,
                    config *cfg_out,
                    uint8_t **obstacles_out,
                    msg_sum_cell_t **summary_out)
{
    if (!path || !cfg_out || !obstacles_out || !summary_out)
        return -1;

    *obstacles_out = NULL;
    *summary_out = NULL;

    FILE *file = fopen(path, "r");
    if (!file)
        return -1;

    memset(cfg_out, 0, sizeof(*cfg_out));

    if (expect_word(file, "WIDTH") != 0 ||
        fscanf(file, "%d", &cfg_out->world_width) != 1)
        goto fail;

    if (expect_word(file, "HEIGHT") != 0 ||
        fscanf(file, "%d", &cfg_out->world_height) != 1)
        goto fail;

    if (expect_word(file, "REPLICATIONS") != 0 ||
        fscanf(file, "%u", &cfg_out->replications) != 1)
        goto fail;

    if (expect_word(file, "MAX_STEPS") != 0 ||
        fscanf(file, "%u", &cfg_out->max_steps) != 1)
        goto fail;

    if (expect_word(file, "PROBS") != 0 ||
        fscanf(file, "%lf %lf %lf %lf",
               &cfg_out->probs.p_up,
               &cfg_out->probs.p_down,
               &cfg_out->probs.p_left,
               &cfg_out->probs.p_right) != 4)
        goto fail;

    int world_type = 0;
    if (expect_word(file, "WORLD_TYPE") != 0 ||
        fscanf(file, "%d", &world_type) != 1)
        goto fail;

    cfg_out->world_type = (world_type_t)world_type;

    if (expect_word(file, "OBSTACLE_DENSITY") != 0 ||
        fscanf(file, "%lf", &cfg_out->obstacle_density) != 1)
        goto fail;

    int width = cfg_out->world_width;
    int height = cfg_out->world_height;
    if (width <= 0 || height <= 0)
        goto fail;

    uint8_t *obstacles = calloc((size_t)(width * height), 1);
    if (!obstacles)
        goto fail;

    if (expect_word(file, "OBSTACLES") != 0)
        goto fail_obstacles;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int value = 0;
            if (fscanf(file, "%d", &value) != 1)
                goto fail_obstacles;

            obstacles[y * width + x] = (value != 0) ? 1 : 0;
        }
    }

    msg_sum_cell_t *summary = calloc((size_t)(width * height), sizeof(*summary));
    if (!summary)
        goto fail_obstacles;

    if (expect_word(file, "SUMMARY") != 0)
        goto fail_summary;

    for (int i = 0; i < width * height; i++) {
        if (fscanf(file, "%lf %lf",
                   &summary[i].avg_steps,
                   &summary[i].probability) != 2)
            goto fail_summary;
    }

    fclose(file);

    *obstacles_out = obstacles;
    *summary_out = summary;
    return 0;

fail_summary:
    free(summary);
fail_obstacles:
    free(obstacles);
fail:
    fclose(file);
    return -1;
}

