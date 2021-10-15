/*********************
 * @file device.h
 * @brief Library supporting network device management.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include "packetio.h"

#include <pcap/pcap.h>

/* Constants */
#define MAX_TRANSMIT_UNIT 1500
#define MAX_DEVICES 255
#define MAX_DEVICE_NAME 255

/* Global Variables */
extern pcap_t *dev_handles[MAX_DEVICES];
extern pcap_if_t *devinfo[MAX_DEVICES];
extern char dev_names[MAX_DEVICES][MAX_DEVICE_NAME];
extern int total_dev;

/**
 * @brief init pcap library
 * *
 * @return 0 on success, -1 on error.
 */
int my_init();

/**
 * @brief Add a device to the library for sending / receiving packets .
 * *
 * @param device Name of network device to send / receive packet on.
 * @return A non - negative _device - ID_ on success , -1 on error .
 */
int addDevice(const char *device);

/**
 * @brief Find a device added by ‘addDevice ‘.
 * *
 * @param device Name of the network device .
 * @return A non - negative _device - ID_ on success , -1 if no such device
 * was found .
 */
int findDevice(const char *device);

#endif