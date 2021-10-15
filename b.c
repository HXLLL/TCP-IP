/*********************
 * @file b.c
 * @brief test sender
 */

#include "device.h"
#include "packetio.h"

#include "utils.h"

#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int callback(const void *data, int len, int dev) {
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
    my_init();
    int ret;

    int a = addDevice("veth2");
    CPE(a == -1, "error adding device", a);

    setFrameReceiveCallback(callback);

    int i=0;
    while(1) {
        usleep(1000000);
        printf("%d\n", ++i);
    }
}