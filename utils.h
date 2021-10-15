#include <stdlib.h>
#include <errno.h>
#define CPES(val, msg)                                                    \
    do                                                                      \
        if (val) {                                                           \
            fprintf(stderr, msg);                                            \
            fprintf(stderr, " Error %d %s , error msg: %d\n", errno, strerror(errno), msg);      \
            exit(1);                                                         \
        }                                                                   \
    while(0)

#define CPE(val, msg, ret)                                                    \
    if (val) {                                                           \
        fprintf(stderr, msg);                                            \
        fprintf(stderr, " Error %d %s , ret value: %d\n", errno, strerror(errno), ret);      \
        exit(1);                                                         \
    }

#define R_CPE(val, ret_value)                                               \
    do                                                                      \
        if (val) { return ret_value; }                                      \
    while(0)
