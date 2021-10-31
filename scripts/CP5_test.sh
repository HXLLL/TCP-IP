#!/bin/bash

if [[ $# == 0 ]]; then
    echo "Usage: $0 [id]"
    exit
fi

case $1 in
    1) sudo ./execNS h1 ../build/router -d veth11 -f ../actions/CP4.txt;;
    2) sudo ./execNS h2 ../build/router -d veth12 -d veth21 -d veth41;;
    3) sudo ./execNS h3 ../build/router -d veth22 -d veth31 -d veth61;;
    4) sudo ./execNS h4 ../build/router -d veth32;;
    5) sudo ./execNS h5 ../build/router -d veth42 -d veth51;;
    6) sudo ./execNS h6 ../build/router -d veth52 -d veth62;;
    *) echo "Bad ID"; exit;;
esac