#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>

#include "net.h"
#include "protocol.h"

typedef struct {
  int x;
  int y;
  unsigned steps;
  unsigned max_dist;
} walker_t;

typedef struct {
  int client_fd;
  volatile int stop;

  uint32_t steps;
  uint32_t max_dist;
  int returned;
} server_context_t;

unsigned count_dist(int x, int y) {
  return (unsigned)(abs(x) + abs(y));
}

void walker_step(walker_t *w) {
  int dir = rand() % 4;

  switch(dir) {
  case 0: w->x++; break;
  case 1: w->x--; break;
  case 2: w->y++; break;
  case 3: w->y--; break;
  }

  w->steps++;

  unsigned d = count_dist(w->x, w->y);
  if (d > w->max_dist) w->max_dist = d;
}

void *sim_thread(void *arg) {
  server_context_t *ctx = (server_context_t *)arg;

  walker_t w = {0};
  srand((unsigned)time(NULL));

  while (!ctx->stop) {
    sleep(1);

    walker_step(&w);

    msg_header_t hdr;
    hdr.type = MSG_STATE;
    hdr.size = sizeof(msg_state_t);

    msg_state_t state;
    state.x = w.x;
    state.y = w.y;
    state.steps = w.steps;
    state.max_dist = w.max_dist;

    ssize_t n1 = send(ctx->client_fd, &hdr, sizeof(hdr), 0);
    ssize_t n2 = send(ctx->client_fd, &state, sizeof(state), 0);

    if (n1 <= 0 || n2 <= 0) {
      printf("[SIM] client disconnected\n");
      break;
    }

    printf("[SIM] (%d,%d) steps=%u max=%u\n", w.x, w.y, w.steps, w.max_dist);
    fflush(stdout);
  }

  ctx->steps = w.steps;
  ctx->max_dist = w.max_dist;
  ctx->returned = (w.x == 0 && w.y ==0) ? 1:0;

  return NULL;
}


int main(void) {
  printf("Server starting...\n");
  fflush(stdout);

  int listen_fd = net_listen_unix("/tmp/test.sock");
  printf("Server listening\n");
  fflush(stdout);

  int client_fd = net_accept(listen_fd);
  printf("Client connected\n");
  fflush(stdout);

  server_context_t ctx;
  ctx.client_fd = client_fd;
  ctx.stop = 0;

  pthread_t sim_tid;
  pthread_create(&sim_tid, NULL, sim_thread, &ctx);

  while (1) {
    msg_header_t hdr;
    ssize_t n = recv(client_fd, &hdr, sizeof(hdr), 0);
    if (n <= 0) {
      printf("[SERVER] client disconnected\n");
      break;
    }

    if (hdr.type == MSG_STOP) {
      printf("[SERVER] STOP received\n");
      ctx.stop = 1;
      break;
    }
  }

  pthread_join(sim_tid, NULL);

  msg_header_t s;
  s.type = MSG_SUMMARY;
  s.size = sizeof(msg_summary_t);

  msg_summary_t sum;
  sum.steps = ctx.steps;
  sum.max_dist = ctx.max_dist;
  sum.returned = ctx.returned;

  send(client_fd, &s, sizeof(s), 0);
  send(client_fd, &sum, sizeof(sum), 0);

  printf("[SERVER] SUMMARY sent\n");

  close(client_fd);
  close(listen_fd);

  printf("Server exiting\n");
  return 0;
}



