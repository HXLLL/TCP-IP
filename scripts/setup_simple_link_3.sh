sudo ./delNS h1
sudo ./delNS h2
sudo ./delNS h3

sudo ./addNS h1
sudo ./addNS h2
sudo ./addNS h3

sudo ./delVeth veth11
sudo ./delVeth veth12
sudo ./delVeth veth21
sudo ./delVeth veth22

sudo ./connectNS h1 h2 veth11 veth12 192.168.1
sudo ./connectNS h2 h3 veth21 veth22 192.168.2
