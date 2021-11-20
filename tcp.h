#ifndef TCP_H
#define TCP_H

#define MAX_PORT 65536

enum SOCKET_TYPE {
    SOCKTYPE_CONNECTION,
    SOCKTYPE_DATA,
};

enum SOCKET_STATE {
    SOCKSTATE_UNBOUNDED,
    SOCKSTATE_BINDED,
    SOCKSTATE_LISTEN,
};

struct socket_info_t {
    int valid;
    int type;
    int state;
    uint32_t addr;
    uint16_t port;
};

struct port_info_t {
    int binded_socket;
};

#endif