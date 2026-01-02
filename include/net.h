#pragma once

int net_listen_unix(const char *path);
int net_accept(int listen_fd);
int net_connect_unix(const char *path);

