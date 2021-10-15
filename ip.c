
#include "ip.h"

int sendIPPacket(const struct in_addr src, const struct in_addr dest,
                 int proto, const void *buf, int len) {
}

typedef int (*IPPacketReceiveCallback)(const void *buf, int len);

int setIPPacketReceiveCallback(IPPacketReceiveCallback callback);

int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                    const void *nextHopMAC, const char *device);