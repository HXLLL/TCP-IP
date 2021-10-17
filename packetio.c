#include "packetio.h"
#include "device.h"
#include "utils.h"

#include <callback.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include <string.h>

int poll_running;
pthread_t poll_handle;
typedef unsigned char u_char;

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

    int ret = pcap_inject(dev_handles[id], send_buffer, total_len);     // TODO: may not be thread safe

    free(send_buffer);

    if (ret >= 0) return 0; // successful => ret=bytes sent (>=0)
    return ret;
}

typedef int (*frameReceiveCallback)(const void *, int, int);

void *poll_func(void *args) {
    struct pcap_pkthdr pkt_header;
    const char *pkt_data;
    frameReceiveCallback callback = (frameReceiveCallback)args;

    while (1) {
        if (poll_running == 0) break;                          // gracefully shutdown
        for (int i = 0; i != total_dev; ++i) {                 // polling
            pkt_data = pcap_next(dev_handles[i], &pkt_header); // non-blocking
            if (pkt_data != NULL) {
                callback(pkt_data, pkt_header.caplen, i);
            }
        }
    }
    return NULL;
}

int setFrameReceiveCallback(frameReceiveCallback callback) {
    int ret;

    if (poll_running) {                        // terminate if poll thread is running
        poll_running = 0;                      //
        ret = pthread_join(poll_handle, NULL); // gracefully shutdown
        RCPE(ret != 0, -1, "Error joining poll thread");
    }

    poll_running = 1;
    ret = pthread_create(&poll_handle, NULL, poll_func, callback); // recreate it with new callback
    RCPE(ret != 0, -1, "Error creating poll thread");

    return 0;
}