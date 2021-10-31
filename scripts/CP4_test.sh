#!/bin/bash

if [[ $# == 0 ]]; then
    echo "Usage: $0 [id]"
    exit
fi

case $1 in
    1) sudo ./execNS h1 ../build/router -d veth11 -f ../actions/CP4.txt;;
    2) sudo ./execNS h2 ../build/router -d veth12 -d veth21;;
    3) sudo ./execNS h3 ../build/router -d veth22 -d veth31;;
    4) sudo ./execNS h4 ../build/router -d veth32;;
    *) echo "Bad ID"; exit;;
esac