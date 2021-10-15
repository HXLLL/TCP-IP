/**
* @file packetio .h
* @brief Library supporting sending / receiving Ethernet II frames .
*/

#include <netinet/ether.h>

/* Constants */
#define BUFFER_SIZE 2048

/* @brief Encapsulate some data into an Ethernet II frame and send it. */
int sendFrame(const void *buf, int len, int ethtype,
              const void *destmac, int id);

/* @brief Process a frame upon receiving it. */
typedef int (*frameReceiveCallback)(const void *, int, int);

/* @brief Register a callback function to be called each time an
 * Ethernet II frame was received . */
int setFrameReceiveCallback(frameReceiveCallback callback);