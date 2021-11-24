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

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_addr.s_addr = 0x0103A8C0;
    local_addr.sin_port = 10000;
    local_addr.sin_family = AF_INET;

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_addr.s_addr = 0x0203A8C0;
    remote_addr.sin_port = 10001;
    remote_addr.sin_family = AF_INET;

    // ret = __wrap_bind(sock_fd, (struct sockaddr*)&local_addr, sizeof(local_addr));
    // CPES(ret < 0);

    ret = __wrap_connect(sock_fd, (struct sockaddr*)&remote_addr, sizeof(remote_addr));
    CPES(ret < 0);

    getchar();

    ret = __wrap_write(sock_fd, "hello", 5);

    getchar();

    __wrap_close(sock_fd);
    CPES(ret < 0);
}