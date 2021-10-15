#include <string.h>
#include "utils.h"
#include "device.h"
#include <pcap/pcap.h>

static char err_buf[PCAP_ERRBUF_SIZE];
static pcap_t *dev_handles[MAX_DEVICES];
static char dev_names[MAX_DEVICES][MAX_DEVICE_NAME];
static int total_dev;

int my_init() {
    int ret;
    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    return ret;
}

int addDevice ( const char * device ) {
    int len=strnlen(device, MAX_DEVICE_NAME);
    if (len>=MAX_DEVICE_NAME || len <= 0) {
        return -1;
    }

    strncpy(dev_names[total_dev], device, MAX_DEVICE_NAME);
    pcap_t *pcap_handle;
    pcap_handle = pcap_create(device, err_buf);
    R_CPE(pcap_handle == NULL, -1);
    dev_handles[total_dev] = pcap_handle;

    return total_dev++;
}

/**
* Find a device added by ‘addDevice ‘.
* *
@param device Name of the network device .
* @return A non - negative _device - ID_ on success , -1 if no such device
* was found .
*/

int findDevice ( const char * device ) {
    for (int i=0;i!=total_dev;++i) {
        if (strncmp(device, dev_names[i], MAX_DEVICE_NAME) == 0) {
            return i;
        }
    }
    return -1;
}
