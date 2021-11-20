/*********************
 * @file a.c
 * @brief test sender
 */

#include "socket.h"

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
    int fd = __wrap_socket(AF_INET, SOCK_STREAM, 0);
    printf("%d\n", fd);
    
    ret = __wrap_listen(65536, 123);
    printf("%d\n", ret);
}