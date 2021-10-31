#!/bin/bash

hosts_cnt=6
link_cnt=6

for i in $(seq 1 $hosts_cnt); do
    sudo ./delNS h$i
    sudo ./addNS h$i
done

for i in $(seq 1 $link_cnt); do
    sudo ./delVeth veth${i}1
    sudo ./delVeth veth${i}2
done

sudo ./connectNS h1 h2 veth11 veth12 192.168.1
sudo ./connectNS h2 h3 veth21 veth22 192.168.2
sudo ./connectNS h3 h4 veth31 veth32 192.168.3
sudo ./connectNS h2 h5 veth41 veth42 192.168.4
sudo ./connectNS h5 h6 veth51 veth52 192.168.5
sudo ./connectNS h3 h6 veth61 veth62 192.168.6

for i in $(seq 1 $hosts_cnt); do
    sudo ./execNS h$i iptables -P INPUT DROP
    sudo ./execNS h$i iptables -P OUTPUT DROP
    sudo ./execNS h$i iptables -P FORWARD DROP
done
