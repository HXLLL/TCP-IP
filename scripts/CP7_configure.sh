#!/bin/bash

sudo ./delNS h1
sudo ./delNS h2
sudo ./delNS h3
sudo ./delNS h4

sudo ./addNS h1
sudo ./addNS h2
sudo ./addNS h3
sudo ./addNS h4

sudo ./delVeth veth11
sudo ./delVeth veth12
sudo ./delVeth veth21
sudo ./delVeth veth22
sudo ./delVeth veth32
sudo ./delVeth veth32

sudo ./connectNS h1 h2 veth11 veth12 192.168.1
sudo ./connectNS h2 h3 veth21 veth22 192.168.2
sudo ./connectNS h3 h4 veth31 veth32 192.168.3

# sudo ./execNS h1 iptables -P INPUT DROP
sudo ./execNS h2 iptables -P INPUT DROP
sudo ./execNS h3 iptables -P INPUT DROP
# sudo ./execNS h4 iptables -P INPUT DROP

# sudo ./execNS h1 iptables -P OUTPUT DROP
sudo ./execNS h2 iptables -P OUTPUT DROP
sudo ./execNS h3 iptables -P OUTPUT DROP
# sudo ./execNS h4 iptables -P OUTPUT DROP

# sudo ./execNS h1 iptables -P FORWARD DROP
sudo ./execNS h2 iptables -P FORWARD DROP
sudo ./execNS h3 iptables -P FORWARD DROP
# sudo ./execNS h4 iptables -P FORWARD DROP
