sudo ./addNS h1
sudo ./addNS h2
sudo ./connectNS h1 h2 veth1 veth2 192.168.3
sudo ./giveAddr veth1 192.168.3.1/24
sudo ./giveAddr veth2 192.168.3.2/24

sudo ./execNS h1 iptables -P INPUT DROP
sudo ./execNS h2 iptables -P INPUT DROP
