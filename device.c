#include "device.h"
#include "packetio.h"
#include "routing_table.h"
#include "utils.h"

#include <pcap/pcap.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <features.h>
#include <net/if.h>

static char err_buf[PCAP_ERRBUF_SIZE];

int total_dev;
static pcap_if_t *alldev;

pcap_t *dev_handles[MAX_DEVICES];
pcap_if_t *devinfo[MAX_DEVICES];
pthread_mutex_t dev_mutex[MAX_DEVICES];

char dev_names[MAX_DEVICES][MAX_DEVICE_NAME];
struct MAC_addr dev_MAC[MAX_DEVICES];
uint32_t dev_IP[MAX_DEVICES];
uint32_t dev_IP_mask[MAX_DEVICE_NAME];

uint32_t get_MAC(int id, struct MAC_addr *res) {
    for (pcap_addr_t *p = devinfo[id]->addresses; p; p = p->next) {
        if (p->addr->sa_family == AF_PACKET) {
            memcpy(res, p->addr->sa_data + 10, ETH_ALEN);
            return 0;
        }
    }
    return -1;
}

uint32_t get_IP(int id, uint32_t *res) {
    for (pcap_addr_t *p = devinfo[id]->addresses; p; p = p->next) {
        if (p->addr->sa_family == AF_INET) {
            memcpy(res, p->addr->sa_data + 2, 4);
            return 0;
        }
    }
    return -1;
}

uint32_t get_IP_mask(int id, uint32_t *res) {
    for (pcap_addr_t *p = devinfo[id]->addresses; p; p = p->next) {
        if (p->addr->sa_family == AF_INET) {
            memcpy(res, p->netmask->sa_data + 2, 4);
            return 0;
        }
    }
    return -1;
}

int device_init() {
    int ret;
    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    RCPE(ret != 0, -1, "Error initiating pcap");

    ret = pcap_findalldevs(&alldev, err_buf);
    RCPE(ret != 0, -1, "Error finding all devs");

    return 0;
}

int addDevice(const char *device) {
    int ret;
    int len = strnlen(device, MAX_DEVICE_NAME);
    if (len >= MAX_DEVICE_NAME || len <= 0) { // invalid device name
        return -1;
    }

    memcpy(dev_names[total_dev], device, len);
    // get devinfo
    devinfo[total_dev] = NULL;
    for (pcap_if_t *p = alldev; p; p = p->next) {
        if (strncmp(device, p->name, MAX_DEVICE_NAME) == 0) {
            devinfo[total_dev] = p;
            break;
        }
    }
    RCPE(devinfo[total_dev] == NULL, -1, "Cannot find device");

    // init mutex
    ret = pthread_mutex_init(&dev_mutex[total_dev], NULL);
    RCPE(ret == -1, -1, "Error initiating mutex");

    pcap_t *pcap_handle;
    pcap_handle = pcap_create(device, err_buf);
    RCPE(pcap_handle == NULL, -1, "Error creating pcap handle");

    ret = pcap_set_snaplen(pcap_handle, 65535);
    RCPE(ret < 0, -1, "Error setting snaplen");

    //    ret = pcap_set_timeout(pcap_handle, 100);
    //    RCPE(ret < 0, -1, "Error setting timeout");

    // get MAC address
    ret = get_MAC(total_dev, &dev_MAC[total_dev]);
    RCPE(ret == -1, -1, "Error getting MAC");

    // get IP address
    ret = get_IP(total_dev, &dev_IP[total_dev]);
    RCPE(ret == -1, -1, "Error getting IP");

    // get IP address
    ret = get_IP_mask(total_dev, &dev_IP_mask[total_dev]);
    RCPE(ret == -1, -1, "Error getting IP mask");

    ret = pcap_set_immediate_mode(pcap_handle, 1);
    RCPE(ret < 0, -1, "Error setting immediate mode");

    ret = pcap_setnonblock(pcap_handle, 1, err_buf);
    RCPE(ret < 0, -1, "Error setting nonblock");

    ret = pcap_activate(pcap_handle); // finally activation
    RCPE(ret < 0, -1, "Error activating handle");

    dev_handles[total_dev] = pcap_handle;
    return total_dev++;
}

int findDevice(const char *device) {
    for (int i = 0; i != total_dev; ++i) {
        if (strncmp(device, devinfo[i]->name, MAX_DEVICE_NAME) == 0) {
            return i;
        }
    }
    return -1;
}
