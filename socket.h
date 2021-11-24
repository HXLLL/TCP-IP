#ifndef SOCKET_H
#define SOCKET_H

/**
* @file socket.h
* @brief POSIX - compatible socket library supporting TCP protocol on
IPv4.
*/
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

int __wrap_socket(int domain, int type, int protocol);

int __wrap_bind(int socket, const struct sockaddr *address,
                socklen_t address_len);
int __wrap_listen(int socket, int backlog);

int __wrap_connect(int socket, const struct sockaddr *address,
                   socklen_t address_len);

int __wrap_accept(int socket, struct sockaddr *address, socklen_t *address_len);

ssize_t __wrap_read(int fildes, void *buf, size_t nbyte);

ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte);

int __wrap_close(int fildes);

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res);

int __real_socket(int domain, int type, int protocol);

int __real_bind(int socket, const struct sockaddr *address,
                socklen_t address_len);
int __real_listen(int socket, int backlog);

int __real_connect(int socket, const struct sockaddr *address,
                   socklen_t address_len);

int __real_accept(int socket, struct sockaddr *address, socklen_t *address_len);

ssize_t __real_read(int fildes, void *buf, size_t nbyte);

ssize_t __real_write(int fildes, const void *buf, size_t nbyte);

int __real_close(int fildes);

int __real_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res);

#endif