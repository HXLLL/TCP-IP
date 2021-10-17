#include "ip.h"
#include "device.h"
#include "utils.h"

#include <unistd.h>
#include <string.h>
#include <getopt.h>

#define MAX_PORT 16

char dev_name[MAX_PORT][MAX_DEVICE_NAME];
int dev_cnt;
int dev_id[MAX_PORT];

int ip_callback(const void *data, int len) {
    printf("Receive data length: %d\n", len);
    len -= ip_hdr_len(data);
    data = ip_raw_content_const(data);
    if (len <= 255) {
        fwrite(data, 1, len, stdout);
    } else {
        fwrite(data, 1, 255, stdout);
    }
    fwrite("\n", 1, 1, stdout);
    return 0;
}


int main(int argc, char *argv[]) {
    int ret, opt;

    while ((opt = getopt(argc, argv, "d:")) != -1) {        // only support -d
        switch (opt) {
        case 'd':
            strncpy(dev_name[dev_cnt], optarg, MAX_DEVICE_NAME);
            ++dev_cnt;
            break;
        default:
            printf("Usage: %s -d [device]\n", argv[0]);
            return -1;
        }
    }

    my_init();
    for (int i=0;i!=dev_cnt;++i) {
        dev_id[i] = addDevice(dev_name[i]);
        CPE(dev_id[i] == -1, "Error adding device", dev_id[i]);
    }

    setIPPacketReceiveCallback(ip_callback);

    while(1) {
        usleep(1000000);
        // report blablabla
    }
}