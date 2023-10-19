#!/bin/bash
ifconfig eth0 172.16.50.1/24
route add -net 172.16.51.0/24 gw 172.16.50.254
route add default gw 172.16.50.254
