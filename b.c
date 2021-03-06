/*********************
 * @file b.c
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
    addr.sin_addr.s_addr = 0x0203A8C0;
    addr.sin_port = 10001;
    addr.sin_family = AF_INET;

    ret = __wrap_bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    CPES(ret < 0);

    ret = __wrap_listen(sock_fd, 0);
    CPES(ret < 0);

    int conn_fd = __wrap_accept(sock_fd, NULL, NULL);
    CPES(conn_fd < 0);

    getchar();

    char buf[100];
    ret = __wrap_read(conn_fd, buf, 5);
    printf("inbound %d bytes\n", ret);
    
    ret = __wrap_close(conn_fd);
    CPES(ret < 0);
    printf("%d\n", ret);
}