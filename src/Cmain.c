#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>

#include "net.h"
#include "protocol.h"

void *recv_thread(void *arg) {
  int fd = *(int *)arg;

  while (1) {
    msg_header_t hdr;
    ssize_t n = recv(fd, &hdr, sizeof(hdr), 0);
    if (n <= 0) {
      printf("Server disconnected\n");
      break;
    }

    if (hdr.type == MSG_STATE) {
      msg_state_t state;
      recv(fd, &state, sizeof(state), 0);
      printf("[RECV] pos=(%d,%d) steps=%u max=%u\n", state.x, state.y, state.steps, state.max_dist);

    } else if (hdr.type == MSG_SUMMARY) {
      msg_summary_t sum;
      recv(fd, &sum, sizeof(sum), 0);

      printf("===SUMMARY===\n");
      printf("steps: %u\n", sum.steps);
      printf("max distance: %u\n", sum.max_dist);
      printf("returned to origin: %s\n", sum.returned ? "yes" : "no");
      break;
    } else if (hdr.type == MSG_STOP) {
      printf("[RECV] server requested stop\n");
      break;
    } else {
      printf("[RECV] unknown msg type %u\n", hdr.type);
    }
  }
  return NULL;
}


int main(void) {
  const char *sock_path = "/tmp/test.sock";

  printf("Client starting...\n");

  int fd = net_connect_unix(sock_path);
  if (fd == -1) {
      fprintf(stderr, "Failed to connect to server\n");
      return 1;
  }

  printf("Connected to server\n");
  pthread_t tid;
  pthread_create(&tid, NULL, recv_thread, &fd);

  sleep(5);

  msg_header_t hdr;
  hdr.type = MSG_STOP;
  hdr.size = 0;

  send(fd, &hdr, sizeof(hdr), 0);
  printf("[CLIENT] STOP sent\n");

  pthread_join(tid, NULL);

  close(fd);
  printf("Client exiting\n");
}

