#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "net.h"

/* Táto trieda bola tvorená za pomoci AI, 
 * konkrétne mi AI pomáhalo opraviť chyby pri počúvaní a príjimani pripojení */

/* vytvorí UNIX socket a začne počúvať na danej ceste */
int net_listen_unix(const char *path)
{
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0); /* lokálna komunikácia, obojsmerný prenos */                    if (sock_fd == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1); /* cesta k suboru, kde sa budu prosey pripajat*/

    /* odstráni starý socket súbor, ak existuje */
    unlink(path);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sock_fd);
        exit(1);
    }

    if (listen(sock_fd, 1) == -1) {
        perror("listen");
        close(sock_fd);
        exit(1);
    }

    return sock_fd;
}

/* prijme nové pripojenie */
int net_accept(int listen_fd)
{
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        exit(1);
    }

    return client_fd;
}

/* pripojí sa k UNIX socketu */
int net_connect_unix(const char *path)
{
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

