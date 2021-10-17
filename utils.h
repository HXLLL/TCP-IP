#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <stdlib.h>

#define ADD_MOD(x, MOD)                                                        \
    do {                                                                       \
        ++x;                                                                   \
        if (x == MOD) x = 0;                                                   \
    } while (0)

#define CPES(val, msg, errmsg)                                                 \
    do                                                                         \
        if (val) {                                                             \
            fprintf(stderr, msg);                                              \
            fprintf(stderr, "%s", errmsg);                                     \
            exit(1);                                                           \
        }                                                                      \
    while (0)

#define CPE(val, msg, ret)                                                     \
    if (val) {                                                                 \
        fprintf(stderr, msg);                                                  \
        exit(1);                                                               \
    }

#define RCPE(val, ret_value, msg)                                              \
    do                                                                         \
        if (val) {                                                             \
            fprintf(stderr, "%s\n", msg);                                      \
            return ret_value;                                                  \
        }                                                                      \
    while (0)

/* Compute checksum for count bytes starting at addr, using one's complement of
 * one's complement sum*/
static unsigned short compute_checksum(unsigned short *addr,
                                       unsigned int count) {
    register unsigned long sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    // if any bytes left, pad the bytes and add
    if (count > 0) {
        sum += ((*addr) & htons(0xFF00));
    }
    // Fold sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    // one's complement
    sum = ~sum;
    return ((unsigned short)sum);
}

static void compute_ip_checksum(struct iphdr *iphdrp) {
    iphdrp->check = 0;
    iphdrp->check =
        compute_checksum((unsigned short *)iphdrp, iphdrp->ihl << 2);
}

inline static void *eth_raw_content(void *buf) { return buf + ETH_HLEN; }
inline static const void *eth_raw_content_const(const void *buf) {
    return buf + ETH_HLEN;
}
inline static void *ip_raw_content(void *buf) {
    struct iphdr *hdr = (struct iphdr *)buf;
    return buf + ((hdr->ihl) << 2);
}
inline static const void *ip_raw_content_const(const void *buf) {
    struct iphdr *hdr = (struct iphdr *)buf;
    return buf + ((hdr->ihl) << 2);
}
inline static int eth_hdr_len(const void *buf) { return ETH_HLEN; }
inline static int ip_hdr_len(const uint8_t *buf) {
    struct iphdr *hdr = (struct iphdr *)buf;
    return (hdr->ihl) << 2;
}
#endif