/*********************
 * @file device.h
 * @brief Library supporting network device management.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include "packetio.h"

#include "common_variable.h"

#include <pthread.h>
#include <pcap/pcap.h>

struct MAC_addr {
    uint8_t data[ETH_ALEN];
};

/* Constants */
#define MAX_TRANSMIT_UNIT 1500
#define MAX_DEVICE_NAME 16

/* Global Variables */
extern pcap_t *dev_handles[MAX_DEVICES];
extern pcap_if_t *devinfo[MAX_DEVICES];
extern char dev_names[MAX_DEVICES][MAX_DEVICE_NAME];
extern struct MAC_addr dev_MAC[MAX_DEVICES];
extern uint32_t dev_IP[MAX_DEVICES];
extern uint32_t dev_IP_mask[MAX_DEVICE_NAME];
extern pthread_mutex_t dev_mutex[MAX_DEVICES];
extern int total_dev;

/**
 * @brief init pcap library
 * *
 * @return 0 on success, -1 on error.
 */
int device_init();

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

uint32_t get_IP(int id, uint32_t *res);
uint32_t get_IP_mask(int id, uint32_t *res);
uint32_t get_MAC(int id, struct MAC_addr *res);

#endif