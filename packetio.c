#include "packetio.h"
#include "device.h"
#include "utils.h"

#include <pcap/pcap.h>
#include <string.h>

static uint8_t send_buffer[BUFFER_SIZE];

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
* *
@param buf Pointer to the payload .
* @param len Length of the payload .
* @param ethtype EtherType field value of this frame .
* @param destmac MAC address of the destination .
* @param id ID of the device ( returned by ‘addDevice ‘) to send on.
* @return 0 on success , -1 on error .
* @see addDevice
*/
int sendFrame(const void *buf, int len, int ethtype,
              const void *destmac, int id) {
    struct ethhdr *hdr = (struct ethhdr *)send_buffer;
    void *data = send_buffer + sizeof(struct ethhdr);
    size_t total_len = len + ETH_HLEN;

    memcpy(&hdr->h_dest, destmac, ETH_ALEN); // assume h_dest is big endian
    memset(&hdr->h_source, 0, ETH_ALEN);     // doesn't need source
    hdr->h_proto = htons(ethtype);           // ethtype is little endian, need conversion
    memcpy(data, buf, len);                  // data need no conversion

    int ret = pcap_inject(dev_handles[id], send_buffer, total_len);
    if (ret >= 0) return 0; // successful => ret=bytes sent
    return ret;
}

/**
* @brief Process a frame upon receiving it.
* *
@param buf Pointer to the frame .
* @param len Length of the frame .
* @param id ID of the device ( returned by ‘addDevice ‘) receiving
* current frame .
* @return 0 on success , -1 on error .
* @see addDevice
*/
typedef int (*frameReceiveCallback)(const void *, int, int);

/**
* @brief Register a callback function to be called each time an
* Ethernet II frame was received .
* *
@param callback the callback function .
* @return 0 on success , -1 on error .
* @see frameReceiveCallback
*/
int setFrameReceiveCallback(frameReceiveCallback callback);