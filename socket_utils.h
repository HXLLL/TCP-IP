#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "utils.h"

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define OPEN_PIPE(f)                                                           \
    do {                                                                       \
        f = fopen(REQ_PIPE_NAME, "w");                                         \
        CPE(f == NULL, "Error opening pipe", 1);                       \
    } while (0);

#endif