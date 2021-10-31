#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MY_CONTROL_PROTOCOl 0xFE
#define PROTO_LINKSTATE 0x01
#define PROTO_ARP_REQUEST 0x02
#define PROTO_ARP_ANSWER 0x03

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

#define DSEND(...) fprintf(stderr, "[SEND] " __VA_ARGS__), fprintf(stderr, "\n")
#define DRECV(...) fprintf(stderr, "[RECV] " __VA_ARGS__), fprintf(stderr, "\n")

static int _hxl_indent_level = 0;
#define INDENT_INC() (++_hxl_indent_level)
#define INDENT_DEC() (--_hxl_indent_level)
#define INDENT_DEBUG(...)                                                      \
    do {                                                                       \
        for (int i = 0; i != _hxl_indent_level; ++i)                           \
            fprintf(stderr, "    ");                                             \
        fprintf(stderr,__VA_ARGS__);                                           \
    } while (0)

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

inline static void print_ip(FILE *file, uint32_t ip) {
    char ip_addr[20];
    inet_ntop(AF_INET, &ip, ip_addr, INET_ADDRSTRLEN);
    fprintf(file, "%s", ip_addr);
}
inline static void print_mac(FILE *file, const uint8_t *mac) {
    fprintf(file, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
}

inline static uint8_t GET1B(const void *base, size_t offset) {
    return *(uint8_t *)(base + offset);
}
inline static uint16_t GET2B(const void *base, size_t offset) {
    return *(uint16_t *)(base + offset);
}
inline static void PUT2B(void *base, size_t offset, uint16_t value) {
    *(uint16_t *)(base + offset) = value;
}
inline static uint32_t GET4B(const void *base, size_t offset) {
    return *(uint32_t *)(base + offset);
}
inline static void PUT4B(void *base, size_t offset, uint32_t value) {
    *(uint32_t *)(base + offset) = value;
}
inline static uint32_t random_ex() {
    static int called = 0;
    if (!called) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        srand(t.tv_nsec);
        called = 1;
    }
    return rand();
}
inline static uint64_t gettime_ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (t.tv_nsec + ((uint64_t)t.tv_sec * (int)1e9)) / 1000000;
}

static char mac_to_str_buf1[255], mac_to_str_buf2[255];
inline static char *mac_to_str(void *mac, int id) {
    if (id == 1) {
        sprintf(mac_to_str_buf1, "%02x:%02x:%02x:%02x:%02x:%02x", GET1B(mac, 0),
                GET1B(mac, 1), GET1B(mac, 2), GET1B(mac, 3), GET1B(mac, 4),
                GET1B(mac, 5));
        return mac_to_str_buf1;
    } else {
        sprintf(mac_to_str_buf2, "%02x:%02x:%02x:%02x:%02x:%02x", GET1B(mac, 0),
                GET1B(mac, 1), GET1B(mac, 2), GET1B(mac, 3), GET1B(mac, 4),
                GET1B(mac, 5));
        return mac_to_str_buf2;
    }
}
#endif