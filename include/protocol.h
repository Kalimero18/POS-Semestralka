#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
	MSG_CONFIG,
	MSG_MODE_CHANGE,
	MSG_INTERACTIVE_STEP,
	MSG_SUMMARY_DATA
} msg_type_t;

typedef enum {
	SIM_MODE_INTERACTIVE,
	SIM_MODE_SUMMARY
} sim_mode_t;

typedef enum {
	DISPLAY_AVG,
	DISPLAY_PROB
} display_type_t;

typedef struct {
	msg_type_t type;
	uint32_t size;
} msg_header_t;

/* krok v interaktívnom móde */
typedef struct {
	int x;
	int y;
	uint32_t step;
	uint32_t replication;
	uint32_t total_replications;
} msg_interactive_step_t;

/* jedna bunka sumárnych dát */
typedef struct {
	double avg_steps;
	double probability;
} msg_summary_cell_t;

#endif

