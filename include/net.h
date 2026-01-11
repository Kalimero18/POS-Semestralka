#pragma once

/* vytvorí UNIX socket a začne počúvať */
int net_listen_unix(const char *path);

/* prijme pripojenie na socket */
int net_accept(int listen_fd);

/* pripojí sa k UNIX socketu */
int net_connect_unix(const char *path);

