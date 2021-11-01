# TCP/IP Protocol Stack
Lab 2 of Computer Network at PKU.

**This version is for part B submission.**

## Major Modules

#### router.c

``Usage: ./build/router -d interface [-d interface ...] [-f action_file]``

```router``` is the main program for part B. It runs ARP protocol (not real ARP, only a simplified one), advertising it's MAC address periodically in neighboring subnets. It runs Link State Routing Algorithm, broadcasting its link state information in the whole network. It handles every packet it received, rebroadcasts it when it is an IP broadcasting packet, forwards it when the packet is for someone else that it knows where to send, and drop it when it doesn't know what to do.

```router``` can run in two modes. If ``` action_file``` is provided, ``router`` will execute the actions sequentially as the ```action_file``` dictates, one action per second. If the ``` action_file ``` is not provided, ``router`` will do routine advertising, handle received packets, and nothing else.

The ``` router``` can either run when Routing Algorithm enabled or disabled. When the Routing Algorithm is enabled, it maintains a global topology, calculates the shortest paths to all other nodes, and updates the routing table automatically according to the result. When the routing algorithm is disabled, routing table need to be set manually by the user via ```action file```, otherwise it wouldn't be able to send or forward any packet.

#### arp.{c,h}

A simplified version of ARP protocol. Actually it doesn't support ARP request and response, and only exchanges IP-MAC correspondence information by broadcasting periodically.

On init, it register link-layer callback function. For every attached network interfaces, it maintains an ARP table, and updates it based on received ARP broadcast packets. All ARP records in an ARP table has a timestamp set on update.

It provide router with a ARP table for every attached network interfaces, which is then used by link state routing algorithm to detect neighboring routers. 

#### link_state.{c,h}

This module implements Link State Routing Algorithm. It maintains its internal data structure based on 1. the ARP table and 2. link state packet broadcast by other routers. Given a destination IP address, It can provide the router with its next_hop mac and port.

On init, it assigns itself with a globally unique gid. Every router in the network can be identified with its gid. Currently this gid is 4 bytes long, and is generated at random when the router initializes. Since the network size is relatively small, the chance of two router generating the same gid is negligible. However, when the network size is big enough (O(sqrt(N)), where N is the number of total possible gid), there will be a significant chance that two routers generates colliding gids in one network. Under that circumstance, one can implement a easy detection mechanism when a router initializes and avoid colliding with other routers, or one can simply change the gid field to 8 bytes long, thereby make the collision probability extremely small for all reasonable network sizes.

Inside the module, it assigns every other router with a local id. It then use this local id to run SPFA algorithm and determine next hop router.

1. Global Link State Table: It is a double-key hash table storing all link state information advertised by other routers. Each record contains the router's attached IPs, attached links, its local id, and its gid. There's also a timestamp in every record, so a router's record will expire if its advertisement doesn't continued to be received. This table is updated based on advertisement sent by other routers.
2. Global IP Table: It is a hash table storing all IPs detected in the network. Given an IP, it can return the corresponding gid. This table is updated based on the Global Link State Table.
3. Neighbor Table: It stores all neighbor's information, whose IP is known in the Global IP Table. This table is based on the ARP table and the Global IP Table.

When a query comes from the router, it first get the destination gid using the destination IP, then get the destination local id, then the next hop local id based on a previously stored SPFA results, then next hop gid, and finally get next hop mac and ip from the Neighbor Table.

#### ip.{c,h}

The IP layer implemented all interfaces as the handout described. It also provides ``` broadcastIPPacket``` function to do IP broadcast.

Inside the module, it maintains a routing table. The routing table is updated through ``setRoutingTable`` which would be called when the user is executing command ``route`` and when the router is updating routing table periodically. On ``sendIPPacket``, it query the routing table, and send the Ethernet Frame based on ``port`` and ``next hop mac`` based on the query result.

#### routing_table.{c,h}

It is implemented using a simple array that can dynamically change its size. It provides the IP layer with the interface to update routing rules based on user/router specification, and the interface to query routing information provided destination IP address. 

On update, the routing table first determine whether this record has been inserted. If so, it update the inserted record, otherwise it expand the record array on demand and insert a new record. On query, the routing table find a record based on the longest prefix match mechanism, and return the result. Every record in the routing table has a  timestamp, it is set when inserted and updated. And on query the expired records will be neglected.

#### packetio.{c,h}

It provide interface for link-layer communication. It implemented ``` sendFrame ``` just as the handout dictated, but **I changed the interface of ```setFrameReceiveCallback```**, allowing the user to specify different callback functions to be invoked upon receiving Ethernet frames containing different *Protocol* field. In addition to that, I also implemented ``` broadcastFrame ```, which is similar to ```sendFrame``` but set Ethernet destination to ```ff:ff:ff:ff:ff:ff``` instead of a specified MAC address.

Inside the module, it maintains a thread that consistently polls all the devices for incoming frames and calls corresponding callback functions when it receives one. Due to the multi-threading design, thread-safety issues must be taken into consideration when developing modules that use it.

#### device.{c,h}

It manages network interfaces attached to the host, and provide interfaces for accessing low-level information of devices. In addition to interfaces required by lab 2 handout, it provide device_init() for initialization, and three helper functions: ```get_IP, get_IP_mask, get_MAC``` for other modules to get address of local devices.

## Other Files

Except for major modules, I wrote/imported several other modules to facilitate my development.

#### utils.h

Common utilities that can be used by all other modules. 

#### debug_utils.h

Utilities used to dump data structures such as link state table, routing table, etc.. Since these functions only depends on the data structures, and are largely decoupled from other functions in their corresponding modules, I decided to place these function in a separate file.

#### scripts/

Bash scripts for configuration, testing and debugging.

#### actions/

Action files read by my router. All action files begin with a number ```n```, followed by ```n``` lines, each representing an action to be executed by router. Currently three actions are supported: ```nop```, ```send``` and ```route```. For their detailed effect and syntax, please refer to ```router.c```.

#### uthash/uthash.h

The hash table library used in my program. It uses C MACRO to implement a generic hash table in pure C. Refer to https://troydhanson.github.io/uthash/ if you are interested.

#### helper/

vnetUtils provided by TAs.

#### main.c, a.c, b.c

Code written for early testing and development. Not relevant.
