#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* typ spustenia simulácie */
typedef enum {
    SIM_NEW  = 1,
    SIM_LOAD = 2
} sim_start_type_t;

/* režim simulácie */
typedef enum {
    SIM_MODE_INTERACTIVE = 1,
    SIM_MODE_SUMMARY     = 2
} sim_mode_t;

/* typ sveta */
typedef enum {
    WORLD_EMPTY     = 1,
    WORLD_OBSTACLES = 2
} world_type_t;

/* pravdepodobnosti pohybu */
typedef struct {
    double p_up;
    double p_down;
    double p_left;
    double p_right;
} probabilities_t;

typedef struct {
    sim_start_type_t start_type;
    sim_mode_t mode;

    world_type_t world_type;
    double obstacle_density;

    int world_width;
    int world_height;

    uint32_t replications;
    uint32_t max_steps;

    probabilities_t probs;

    char input_file[256];
    char output_file[256];
} config;

#endif

