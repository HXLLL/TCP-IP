#include "utils.h"

#include <stdlib.h>
#include <errno.h>
#include <pcap/pcap.h> 
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) { 
    char err_buf[PCAP_ERRBUF_SIZE];
    int ret;
    char *devname;

    if (argc > 1) {
        devname = argv[1];
    } else {
        devname = "any";
    }

    ret = pcap_init(PCAP_CHAR_ENC_LOCAL, NULL);
    CPES(ret==-1, err_buf);

//    pcap_if_t *all_devs;
//    ret = pcap_findalldevs(&all_devs, err_buf);
//    CPES(ret==-1, err_buf);
//    for (pcap_if_t *i=all_devs;i;i=i->next)
//        printf("%s\n", i->name);
//    pcap_freealldevs(all_devs);

    pcap_t *pcap_handle;
    pcap_handle = pcap_create(devname, err_buf);
    CPES(pcap_handle == NULL, err_buf);
    ret = pcap_activate(pcap_handle);
    CPE(ret != 0, "Error activating", ret);

    int *dlt_buf;
    ret = pcap_list_datalinks(pcap_handle, &dlt_buf);
    CPE(ret < 0, "Error gettting datalinks", ret);
    for (int i=0;i!=ret;++i) {
        printf("%s\n", pcap_datalink_val_to_name(dlt_buf[i]));
        printf("%s\n", pcap_datalink_val_to_description(dlt_buf[i]));
    }
    pcap_free_datalinks(dlt_buf);

    pcap_close(pcap_handle);

    return 0;
}