#include "ip.h"
#include "device.h"
#include "packetio.h"
#include "routing_table.h"
#include "uthash/uthash.h"
#include "utils.h"

#include <callback.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <string.h>

struct RT *rt;

const int IP_DEBUG = 0;

int ip_init() {
    int ret;

    rt = (struct RT *)malloc(sizeof(struct RT));
    ret = rt_init(rt);
    RCPE(ret < 0, -1, "Error initiating routing table");

    return 0;
}

int sendIPPacket(const struct in_addr src, const struct in_addr dest, int proto,
                 const void *buf, int len) {
    int ret;
    size_t total_len = sizeof(struct iphdr) + len;
    uint8_t *send_buffer = malloc(total_len);
    struct iphdr *hdr = (struct iphdr *)send_buffer;

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

    void *data = ip_raw_content(send_buffer);
    memcpy(data, buf, len);

    struct Record rec;
    ret = rt_query(rt, dest, &rec);
    if (ret != 1) return -1;
    sendFrame(send_buffer, total_len, ETH_P_IP, &rec.nexthop_mac, rec.device);

    if (IP_DEBUG) {
        char src_ip[20], dst_ip[20];
        inet_ntop(AF_INET, &hdr->saddr, src_ip, 20);
        inet_ntop(AF_INET, &hdr->daddr, dst_ip, 20);
        fprintf(stderr, "send packet, from port %d, %s ---> %s\n", rec.device, src_ip, dst_ip);
    }

    free(send_buffer);
    return 0;
}

int broadcastIPPacket(const struct in_addr src, int proto, const void *buf,
                      int len, uint16_t broadcast_id) {
    size_t total_len = sizeof(struct iphdr) + len;
    uint8_t *send_buffer = malloc(total_len);
    struct iphdr *hdr = (struct iphdr *)send_buffer;

    hdr->version = 4;
    hdr->ihl = 5;
    hdr->tos = 0;
    hdr->tot_len = htons(total_len);

    hdr->id = broadcast_id;
    hdr->frag_off = 0;

    hdr->ttl = 5;
    hdr->protocol = proto;

    hdr->saddr = src.s_addr;
    hdr->daddr = 0xffffffff;

    struct MAC_addr broadcast_mac;
    memset(&broadcast_mac, 0xff, sizeof(broadcast_mac));

    compute_ip_checksum(hdr);

    void *data = ip_raw_content(send_buffer);
    memcpy(data, buf, len);

    for (int i = 0; i != total_dev; ++i) {
        sendFrame(send_buffer, total_len, ETH_P_IP, broadcast_mac.data, i);
    }

    if (IP_DEBUG) {
        fprintf(stderr, "broadcast packet, %x ---> %x\n", hdr->saddr,
                hdr->daddr);
    }

    free(send_buffer);
    return 0;
}

typedef int (*IPPacketReceiveCallback)(const void *buf, int len);

void IPCallbackWrapper(void *data, va_alist valist) {
    int ret;
    IPPacketReceiveCallback ip_cb = (IPPacketReceiveCallback)data;
    void *eth_frame, *ip_frame;
    int len, dev_id;

    va_start_int(valist);
    eth_frame = va_arg_ptr(valist, void *);
    len = va_arg_int(valist);
    dev_id = va_arg_int(valist);

    ip_frame = eth_raw_content(eth_frame);
    len -= eth_hdr_len(eth_frame);

    ret = ip_cb(ip_frame, len);

    va_return_int(valist, ret);
}

int setIPPacketReceiveCallback(IPPacketReceiveCallback callback) {
    callback_t cb;
    cb = alloc_callback(IPCallbackWrapper, callback);
    setFrameReceiveCallback(cb, ETH_P_IP);
    return 0;
}

int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                    const void *nextHopMAC, const char *device) {
    int ret;
    uint64_t cur_time = gettime_ms();

    int dev_id = findDevice(device);

    struct Record rec = {.dest = dest,
                         .mask = mask,
                         .device = findDevice(device),
                         .timestamp = cur_time};

    memcpy(rec.nexthop_mac, nextHopMAC, 6); // assume nextHopMAC is big endian

    ret = rt_update(rt, &rec);

    return ret;
}