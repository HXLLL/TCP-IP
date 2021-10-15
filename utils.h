#include <errno.h>
#include <stdlib.h>
#define CPES(val, msg, errmsg)             \
    do                                     \
        if (val) {                         \
            fprintf(stderr, msg);          \
            fprintf(stderr, "%s", errmsg); \
            exit(1);                       \
        }                                  \
    while (0)

#define CPE(val, msg, ret)    \
    if (val) {                \
        fprintf(stderr, msg); \
        exit(1);              \
    }

#define RCPE(val, ret_value)  \
    do                        \
        if (val) {            \
            return ret_value; \
        }                     \
    while (0)
