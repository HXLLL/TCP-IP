/**
* @file device .h
* @brief Library supporting network device management .
*/

#ifndef DEVICE_H
#define DEVICE_H

#include <pcap/pcap.h>

/* Constants */
#define MAX_DEVICES 255
#define MAX_DEVICE_NAME 255

/* Global Variables */
extern pcap_t *dev_handles[MAX_DEVICES];
extern pcap_if_t *devinfo[MAX_DEVICES];
extern char dev_names[MAX_DEVICES][MAX_DEVICE_NAME];
extern int total_dev;

/* init pcap library */
int my_init();

/* Add a device to the library for sending / receiving packets */
int addDevice(const char *device);

/* Find a device added by ‘addDevice ‘ */
int findDevice(const char *device);

#endif