/*********************
 * @file a.c
 * @brief test sender
 */

#include "ip.h"
#include "device.h"
#include "packetio.h"

#include "utils.h"

#include <callback.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>

int main() {
    my_init();
    int ret;

    int a = addDevice("veth1");
    CPE(a == -1, "error adding device", a);

    char *content = "hi i'm bob";
    int len = strlen(content);
    uint8_t dest_mac[6] = {0x22, 0xce, 0x5b, 0xfb, 0xe6, 0x05};

    struct in_addr src,dest, mask;
    inet_pton(AF_INET, "192.168.3.1", &src);
    inet_pton(AF_INET, "255.255.255.255", &dest);
    inet_pton(AF_INET, "255.255.255.255", &mask);

    ret = setRoutingTable(dest, mask, dest_mac, "veth1");
    CPE(ret < 0, "Error setting routing table", ret);

    ret = sendIPPacket(src, dest, 6, content, len);
    CPE(ret < 0, "Error sending packet", ret);
}