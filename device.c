#include "device.h"
#include "ip.h"
#include "packetio.h"
#include "routing_table.h"
#include "utils.h"

#include <pthread.h>
#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <features.h>
#include <net/if.h>

static char err_buf[PCAP_ERRBUF_SIZE];
static pcap_if_t *alldev;

pcap_t *dev_handles[MAX_DEVICES];
pcap_if_t *devinfo[MAX_DEVICES];
struct RT *dev_rt[MAX_DEVICES];
struct MAC_addr dev_MAC[MAX_DEVICES];
pthread_mutex_t dev_mutex[MAX_DEVICES];
int total_dev;

/*****
 * get mac address
 * TODO: ugly, rewrite it
 ***/
int get_MAC(const char *device, struct MAC_addr *res) {
    int ret;
    char filename[255];
    sprintf(filename, "/sys/class/net/%s/address", device);
    FILE *addr_sysfile = fopen(filename, "r");
    RCPE(addr_sysfile == NULL, -1, "Error opening MAC address file");
    ret = fscanf(addr_sysfile, "%x:%x:%x:%x:%x:%x", &res->data[0], &res->data[1], &res->data[2], &res->data[3], &res->data[4], &res->data[5]);
    RCPE(ret != 6, -1, "Error reading MAC address");
    fclose(addr_sysfile);
    return 0;
}

/*****
 * TODO: too ugly
 ****/
int get_IP(int id, struct sockaddr *res) {
    for (pcap_addr_t *p=devinfo[id]->addresses; p; p=p->next) {
        if (p->addr->sa_family == AF_INET) {
            memcpy(res, p->addr, sizeof(struct sockaddr));
            return 0;
        }
    }
    return -1;
}

int my_init() {
    int ret;
    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    RCPE(ret != 0, -1, "Error initiating pcap");

    ret = pcap_findalldevs(&alldev, err_buf);
    RCPE(ret != 0, -1, "Error finding all devs");

    ip_init();

    return 0;
}

int addDevice(const char *device) {
    int ret;
    int len = strnlen(device, MAX_DEVICE_NAME);
    if (len >= MAX_DEVICE_NAME || len <= 0) { // invalid device name
        return -1;
    }

    // get devinfo
    for (pcap_if_t *p = alldev; p; p = p->next) {
        if (strncmp(device, p->name, MAX_DEVICE_NAME) == 0) {
            devinfo[total_dev] = p;
            break;
        }
    }

    // get MAC address
    ret = get_MAC(device, &dev_MAC[total_dev]);
    RCPE(ret == -1, -1, "Error getting MAC");
    
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
