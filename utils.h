#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define CPES(val)                                                              \
    do                                                                         \
        if (val) {                                                             \
            fprintf(stderr, "[%s:%d] %d:%s", __FILE__, __LINE__, errno,        \
                    strerror(errno));                                          \
            exit(1);                                                           \
        }                                                                      \
    while (0)

#define CPEL(val)                                                              \
    if (val) {                                                                 \
        fprintf(stderr, "[ERROR] %s:%d\t", __FILE__, __LINE__);                \
        exit(1);                                                               \
    }

#define CPE(val, msg, ret)                                                     \
    if (val) {                                                                 \
        fprintf(stderr, "%s:%d\t %s", __FILE__, __LINE__, msg);                \
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
#define DPIPE(...) fprintf(stderr, "[PIPE] " __VA_ARGS__), fprintf(stderr, "\n")

static int _hxl_indent_level = 0;
#define INDENT_INC() (++_hxl_indent_level)
#define INDENT_DEC() (--_hxl_indent_level)
#define INDENT_DEBUG(...)                                                      \
    do {                                                                       \
        for (int i = 0; i != _hxl_indent_level; ++i)                           \
            fprintf(stderr, "    ");                                           \
        fprintf(stderr, __VA_ARGS__);                                          \
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
static void compute_tcp_checksum(struct tcphdr *tcphdrp) {
    tcphdrp->check = 0;
    tcphdrp->check =
        compute_checksum((unsigned short *)tcphdrp, tcphdrp->doff << 2);
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
    const struct iphdr *hdr = (const struct iphdr *)buf;
    return buf + ((hdr->ihl) << 2);
}
inline static int eth_hdr_len(const void *buf) { return ETH_HLEN; }
inline static int ip_hdr_len(const uint8_t *buf) {
    const struct iphdr *hdr = (const struct iphdr *)buf;
    return (hdr->ihl) << 2;
}
inline static int tcp_hdr_len(const void *buf) {
    const struct tcphdr *hdr = (const struct tcphdr *)buf;
    return ((hdr->doff) << 2);
}
inline static const void *tcp_raw_content_const(const void *buf) {
    const struct tcphdr *hdr = (const struct tcphdr *)buf;
    return buf + ((hdr->doff) << 2);
}
inline static void *tcp_raw_content(void *buf) {
    struct tcphdr *hdr = (struct tcphdr *)buf;
    return buf + ((hdr->doff) << 2);
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
/**
 * @brief
 * ! do not call it with the same id in one single function call!!!
 */
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

struct ring_buffer_t {
    void **data;
    size_t cap, size;
    int head, tail;
};
static struct ring_buffer_t *rb_new(size_t len) {
    struct ring_buffer_t *r = malloc(sizeof(struct ring_buffer_t));
    r->data = malloc(sizeof(void *) * len);
    memset(r->data, 0, sizeof(void *) * len);
    r->cap = len;
    r->size = 0;
    r->head = r->tail = 0;
    return r;
}

static void rb_free(struct ring_buffer_t *rb) {
    free(rb->data);
    free(rb);
}

static int rb_push(struct ring_buffer_t *rb, void *value) {
    if (rb->size == rb->cap) exit(1); // TODO: return -1
    ++rb->size;
    rb->data[rb->tail] = value;
    ADD_MOD(rb->tail, rb->cap);
    return 0;
}

static int rb_pop(struct ring_buffer_t *rb) {
    if (!rb->size) exit(1); // TODO: return -1
    --rb->size;
    ADD_MOD(rb->head, rb->cap);
    return 0;
}

static void *rb_front(struct ring_buffer_t *rb) { return rb->data[rb->head]; }
static int rb_empty(struct ring_buffer_t *rb) { return !rb->size; }

static void *rb_nxt(struct ring_buffer_t *rb, void **p) {
    if (p == rb->data + rb->tail) exit(1); // TODO: return -1
    ++p;
    if (p == rb->data + rb->cap) p = rb->data;
    return p;
}

#endif