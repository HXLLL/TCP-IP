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
}