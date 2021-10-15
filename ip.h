/**
* @file ip.h
* @brief Library supporting sending/receiving IP packets encapsulated
in an Ethernet II frame.
*/

#ifndef IP_H
#define IP_H

#include <netinet/ip.h>

/**
* @brief Send an IP packet to specified host.
* *
* @param src Source IP address.
* @param dest Destination IP address.
* @param proto Value of 'protocol' field in IP header.
* @param buf pointer to IP payload.
* @param len Length of IP payload.
* @return 0 on success, -1 on error.
*/
int sendIPPacket(const struct in_addr src, const struct in_addr dest,
                 int proto, const void *buf, int len);

/**
* @brief Process an IP packet upon receiving it.
* *
* @param buf Pointer to the packet.
* @param len Length of the packet.
* @return 0 on success, -1 on error.
* @see addDevice
*/
typedef int (*IPPacketReceiveCallback)(const void *buf, int len);

/**
* @brief Register a callback function to be called each time an IP
packet was received.
* *
* @param callback The callback function.
* @return 0 on success, -1 on error.
* @see IPPacketReceiveCallback
*/
int setIPPacketReceiveCallback(IPPacketReceiveCallback callback);

/**
* @brief Manully add an item to routing table. Useful when talking
with real Linux machines.
* *
* @param dest The destination IP prefix.
* @param mask The subnet mask of the destination IP prefix.
* @param nextHopMAC MAC address of the next hop.
* @param device Name of device to send packets on.
* @return 0 on success, -1 on error
*/
int setRoutingTable(const struct in_addr dest, const struct in_addr mask,
                    const void *nextHopMAC, const char *device);

#endif