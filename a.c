#include "device.h"

#include <stdio.h>

int main() {
    my_init();

    int a = addDevice("wlan0");

    printf("%d\n", findDevice("wlan0"));
}