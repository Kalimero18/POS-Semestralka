#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* typy správ medzi klientom a serverom */
typedef enum {
    MSG_CONFIG            = 1,
    MSG_INTERACTIVE_STEP  = 2,
    MSG_SUMMARY_DATA      = 3,
    MSG_OBSTACLES         = 4
} msg_type_t;

/* hlavička správy */
typedef struct {
    msg_type_t type;
    uint32_t size;
} msg_header_t;

/* interaktívny */
typedef struct {
    int x;
    int y;
    uint32_t step;
    uint32_t replication;
    uint32_t total_replications;
} msg_int_t;

/* sumarizačné dáta */
typedef struct {
    double avg_steps;
    double probability;
} msg_sum_cell_t;

#endif

