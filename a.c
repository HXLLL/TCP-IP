/*********************
 * @file a.c
 * @brief test sender
 */

#include "socket.h"

#include "common_variable.h"
#include "utils.h"

#include <callback.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int main() {

    int ret;
    int sock_fd = __wrap_socket(AF_INET, SOCK_STREAM, 0);
    printf("%d\n", sock_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_port = 10000;
    addr.sin_family = AF_INET;

    ret = __wrap_bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));

    printf("%d\n", ret);

    ret = __wrap_listen(65536, 123);
    printf("%d\n", ret);
}