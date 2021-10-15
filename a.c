#include "device.h"
#include "packetio.h"

#include "utils.h"

#include <netinet/ether.h>
#include <stdio.h>
#include <string.h>

int main() {
    my_init();
    int ret;

    int a = addDevice("veth1");
    CPE(a == -1, "error adding device", a);

    char *content = "Hello World!";
    int len = strlen(content);
    uint8_t dest_mac[6] = {0x22, 0xce, 0x5b, 0xfb, 0xe6, 0x05};

    ret = sendFrame(content, len, 0x0800, dest_mac, a);
    CPES(ret < 0, "Error sending data", pcap_geterr(dev_handles[a]));
    printf("Send %d bytes\n", ret);
}