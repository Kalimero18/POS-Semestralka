#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "net.h"

int net_listen_unix(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  unlink(path);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(fd);
    exit(1);
  }

  if (listen(fd, 1) == -1) {
    perror("listen");
    close(fd);
    exit(1);
  }

  return fd;
}

int net_accept(int listen_fd) {
  int fd = accept(listen_fd, NULL, NULL);
  if (fd == -1) {
    perror("accept");
    exit(1);
  }
  return fd;
}

int net_connect_unix(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("connect");
    close(fd);
    return -1;
  }

  return fd;
}


