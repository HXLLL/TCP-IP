/*********************
 * @file b.c
 * @brief test sender
 */

#include "socket.h"

#include "utils.h"

#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int ip_callback(const void *data, int len) {
    printf("Receive data length: %d\n", len);
    len -= ip_hdr_len(data);
    data = ip_raw_content_const(data);
    if (len <= 255) {
        fwrite(data, 1, len, stdout);
    } else {
        fwrite(data, 1, 255, stdout);
    }
    fwrite("\n", 1, 1, stdout);
    return 0;
}

int ether_callback(const void *data, int len, int dev) {
    printf("Receive data from dev %d, length: %d\n", dev, len);
    if (len <= 255) {
        fwrite(data + ETH_HLEN, 1, len - ETH_HLEN, stdout);
    } else {
        fwrite(data + ETH_HLEN, 1, 255 - ETH_HLEN, stdout);
    }
    fwrite("\n", 1, 1, stdout);
    return 0;
}

int main() {
    FILE *f = fopen("1","r");
    char buf[20];
    int cnt;
    while (1) {
        int a,b;
        cnt = fscanf(f, "%d%d",&a, &b);
        if (cnt == 2) {
            printf("%d\n", a+b);
        }
    }
}