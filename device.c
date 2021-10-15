#include "device.h"

#include <pcap/pcap.h>
#include <string.h>

#include "utils.h"

static char err_buf[PCAP_ERRBUF_SIZE];
static pcap_if_t *alldev;

pcap_t *dev_handles[MAX_DEVICES];
pcap_if_t *devinfo[MAX_DEVICES];
int total_dev;

/**
 * init pcap library
 * *
 * @return 0 on success, -1 on error.
 */
int my_init() {
    int ret;
    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    if (ret != 0) return ret;

    ret = pcap_findalldevs(&alldev, err_buf);
    if (ret != 0) return ret;
    return ret;
}

/**
* Add a device to the library for sending / receiving packets .
* *
@param device Name of network device to send / receive packet on.
* @return A non - negative _device - ID_ on success , -1 on error .
*/
int addDevice(const char *device) {
    int ret;
    int len = strnlen(device, MAX_DEVICE_NAME);
    if (len >= MAX_DEVICE_NAME || len <= 0) { // invalid device name
        return -1;
    }
    strncpy(dev_names[total_dev], device, MAX_DEVICE_NAME);

    for (pcap_if_t *p = alldev; p; p = p->next) {
        if (strncmp(device, p->name, MAX_DEVICE_NAME) == 0) {
            devinfo[total_dev] = p;
            break;
        }
    }

    pcap_t *pcap_handle;
    pcap_handle = pcap_create(device, err_buf);
    RCPE(pcap_handle == NULL, -1);
    dev_handles[total_dev] = pcap_handle;

    ret = pcap_activate(dev_handles[total_dev]);
    RCPE(ret < 0, -1);

    return total_dev++;
}

/**
* Find a device added by ‘addDevice ‘.
* *
@param device Name of the network device .
* @return A non - negative _device - ID_ on success , -1 if no such device
* was found .
*/
int findDevice(const char *device) {
    for (int i = 0; i != total_dev; ++i) {
        if (strncmp(device, dev_names[i], MAX_DEVICE_NAME) == 0) {
            return i;
        }
    }
    return -1;
}
