#pragma once

#include <stdint.h>

typedef enum {
  MSG_HELLO = 1,
  MSG_STATE = 2,
  MSG_STOP  = 3,
  MSG_SUMMARY = 4
} msg_type_t;


typedef struct {
  uint32_t type;
  uint32_t size;
} msg_header_t;

typedef struct {
  int x;
  int y;
  uint32_t steps;
  uint32_t max_dist;
} msg_state_t;

typedef struct {
  uint32_t steps;
  uint32_t max_dist;
  int returned;
} msg_summary_t;

