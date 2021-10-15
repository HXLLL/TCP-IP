#include "device.h"
#include "packetio.h"

#include <pcap/pcap.h>
#include <string.h>

#include "utils.h"

static char err_buf[PCAP_ERRBUF_SIZE];
static pcap_if_t *alldev;

pcap_t *dev_handles[MAX_DEVICES];
pcap_if_t *devinfo[MAX_DEVICES];
int total_dev;

int my_init() {
    int ret;
    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    if (ret != 0) return ret;

    ret = pcap_findalldevs(&alldev, err_buf);
    if (ret != 0) return ret;
    return ret;
}

int addDevice(const char *device) {
    int ret;
    int len = strnlen(device, MAX_DEVICE_NAME);
    if (len >= MAX_DEVICE_NAME || len <= 0) { // invalid device name
        return -1;
    }

    for (pcap_if_t *p = alldev; p; p = p->next) {
        if (strncmp(device, p->name, MAX_DEVICE_NAME) == 0) {
            devinfo[total_dev] = p;
            break;
        }
    }

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

    ret = pcap_activate(pcap_handle);   // finally activation
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
