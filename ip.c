#include "device.h"
#include "ip.h"
#include "utils.h"
#include "packetio.h"
#include "routing_table.h"

#include <string.h>
#include <netinet/ip.h>
#include <netinet/ether.h>

struct RT *rt;

int ip_init() {
    int ret;

    rt = (struct RT*)malloc(sizeof(struct RT));
    ret = rt_init(rt);
    RCPE(ret < 0, -1, "Error initiating routing table");

    return 0;
}

int sendIPPacket(const struct in_addr src, const struct in_addr dest,
                 int proto, const void *buf, int len) {
    size_t total_len = sizeof(struct iphdr) + len;
    uint8_t *send_buffer = malloc(total_len);
    struct iphdr *hdr = (struct iphdr*)send_buffer;
    void *data = send_buffer + sizeof(struct iphdr);

    hdr->version = 4;
    hdr->ihl = 5;
    hdr->tos = 0;
    hdr->tot_len = htons(total_len);

    hdr->id = 0;
    hdr->frag_off = 0;

    hdr->ttl = 5;
    hdr->protocol = proto;

    hdr->saddr = src.s_addr;
    hdr->daddr = dest.s_addr;

    compute_ip_checksum(hdr);

    memcpy(data, buf, len);

    struct Record rec;
    rt_query(rt, dest, &rec);
    sendFrame(send_buffer, total_len, ETH_P_IP, rec.nexthop_mac, rec.device);

    free(send_buffer);
    return 0;
}

typedef int (*IPPacketReceiveCallback)(const void *buf, int len);

int setIPPacketReceiveCallback(IPPacketReceiveCallback callback);

int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                    const void *nextHopMAC, const char *device) {
    int ret;

    struct Record rec = { .dest=dest, .mask = mask, .device = findDevice(device) };
    memcpy(rec.nexthop_mac, nextHopMAC, 6);             // assume nextHopMAC is big endian

    ret = rt_update(rt, &rec);

    return 0;
}