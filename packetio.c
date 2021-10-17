#include "packetio.h"
#include "device.h"
#include "utils.h"
#include "uthash/uthash.h"
#include "arp.h"

#include <unistd.h>
#include <time.h>
#include <callback.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include <string.h>

int daemon_running;
pthread_t daemon_handle;
typedef unsigned char u_char;
frameReceiveCallback link_layer_callback[65536];

int sendFrame(const void *buf, int len, int ethtype,
              const void *destmac, int id) {
    RCPE(len > MAX_TRANSMIT_UNIT, -1, "Frame too large");

    size_t total_len = len + sizeof(struct ethhdr);
    uint8_t *send_buffer = malloc(total_len);
    struct ethhdr *hdr = (struct ethhdr *)send_buffer;
    void *data = send_buffer + sizeof(struct ethhdr);

    memcpy(&hdr->h_dest, destmac, ETH_ALEN); // assume h_dest is big endian
    memcpy(&hdr->h_source, &dev_MAC[id], ETH_ALEN);
    hdr->h_proto = htons(ethtype);           // ethtype is little endian, need conversion
    memcpy(data, buf, len);                  // data need no conversion

    pthread_mutex_lock(&dev_mutex[id]);
    int ret = pcap_inject(dev_handles[id], send_buffer, total_len);
    pthread_mutex_unlock(&dev_mutex[id]);

    free(send_buffer);

    if (ret >= 0) return 0; // successful => ret=bytes sent (>=0)
    return ret;
}

int broadcastFrame(const void *buf, int len, int ethtype, int id) {
    RCPE(len > MAX_TRANSMIT_UNIT, -1, "Frame too large");

    size_t total_len = len + sizeof(struct ethhdr);
    uint8_t *send_buffer = malloc(total_len);
    struct ethhdr *hdr = (struct ethhdr *)send_buffer;
    void *data = send_buffer + sizeof(struct ethhdr);

    memset(&hdr->h_dest, 0xff, ETH_ALEN); // assume h_dest is big endian
    memcpy(&hdr->h_source, &dev_MAC[id], ETH_ALEN);
    hdr->h_proto = htons(ethtype);           // ethtype is little endian, need conversion
    memcpy(data, buf, len);                  // data need no conversion

    pthread_mutex_lock(&dev_mutex[id]);
    int ret = pcap_inject(dev_handles[id], send_buffer, total_len);
    pthread_mutex_unlock(&dev_mutex[id]);

    free(send_buffer);

    if (ret >= 0) return 0; // successful => ret=bytes sent (>=0)
    return ret;
}

typedef int (*frameReceiveCallback)(const void *, int, int);

void poll_packet() {
    struct pcap_pkthdr pkt_header;
    const void *pkt_data;
    for (int i = 0; i != total_dev; ++i) {                 // polling
        pkt_data = pcap_next(dev_handles[i], &pkt_header); // non-blocking
        if (pkt_data != NULL) {
            struct ethhdr *hdr = (struct ethhdr*)pkt_data;
            if (link_layer_callback[hdr->h_proto])
                link_layer_callback[hdr->h_proto](pkt_data, pkt_header.caplen,
                                                  i);
        }
    }
}

void *link_layer_daemon(void *args) {
    int ret;

    struct timespec ARP_last, ARP_now;
    double ARP_every = 1;

    while (1) {
        if (daemon_running == 0) break;                          // gracefully shutdown

        clock_gettime(CLOCK_MONOTONIC, &ARP_now);
        if (ARP_now.tv_sec - ARP_last.tv_sec >= ARP_every) {
            ret = ARP_advertise();                              // need to move to ARP
            RCPE(ret == -1, NULL, "Error advertising ARP");

            ARP_last = ARP_now;
        }

        poll_packet();

    }
    free(args);
    return NULL;
}

int setFrameReceiveCallback(frameReceiveCallback callback, uint16_t protocol) {
    int ret;

    if (daemon_running) {                        // terminate if poll thread is running
        daemon_running = 0;                      //
        ret = pthread_join(daemon_handle, NULL); // gracefully shutdown
        RCPE(ret != 0, -1, "Error joining poll thread");
    }

    link_layer_callback[protocol] = callback;

    daemon_running = 1;
    ret = pthread_create(&daemon_handle, NULL, link_layer_daemon, NULL); // recreate it with new callback
    RCPE(ret != 0, -1, "Error creating poll thread");

    return 0;
}