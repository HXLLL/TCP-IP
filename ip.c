#include "uthash/uthash.h"
#include "device.h"
#include "ip.h"
#include "utils.h"
#include "packetio.h"
#include "routing_table.h"
#include "arp.h"

#include <callback.h>
#include <pthread.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/ether.h>

struct RT *rt;

int ip_init() {
    int ret;

    rt = (struct RT*)malloc(sizeof(struct RT));
    ret = rt_init(rt);
    RCPE(ret < 0, -1, "Error initiating routing table");

    ret = arp_init();
    RCPE(ret == -1, -1, "Error initializing ARP");

    return 0;
}

int sendIPPacket(const struct in_addr src, const struct in_addr dest, const struct in_addr next_hop,
                 int proto, const void *buf, int len) {
    size_t total_len = sizeof(struct iphdr) + len;
    uint8_t *send_buffer = malloc(total_len);
    struct iphdr *hdr = (struct iphdr*)send_buffer;

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

    struct arp_record *rec;
    arp_query(next_hop.s_addr);
    sendFrame(send_buffer, total_len, ETH_P_IP, &rec->mac_addr, rec->port);

    free(send_buffer);
    return 0;
}

int broadcastIPPacket(const struct in_addr src, int proto, const void *buf, int len, uint16_t broadcast_id) {
    size_t total_len = sizeof(struct iphdr) + len;
    uint8_t *send_buffer = malloc(total_len);
    struct iphdr *hdr = (struct iphdr*)send_buffer;

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

    compute_ip_checksum(hdr);

    void *data = ip_raw_content(send_buffer);
    memcpy(data, buf, len);

    struct arp_record *rec, *tmp;
    HASH_ITER(hh, arp_table, rec, tmp) {
        sendFrame(send_buffer, total_len, ETH_P_IP, &rec->mac_addr, rec->port);
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
    eth_frame = va_arg_ptr(valist, void*);
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
}

int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                    const void *nextHopMAC, const char *device) {
    int ret;

    struct Record rec = { .dest=dest, .mask = mask, .device = findDevice(device) };
    memcpy(rec.nexthop_mac, nextHopMAC, 6);             // assume nextHopMAC is big endian

    ret = rt_update(rt, &rec);

    return 0;
}