#!/bin/bash

hosts_cnt=3
link_cnt=2

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

for i in $(seq 1 $hosts_cnt); do
    sudo ./execNS h$i iptables -P INPUT DROP
    sudo ./execNS h$i iptables -P OUTPUT DROP
    sudo ./execNS h$i iptables -P FORWARD DROP
done
