# Cisco Router Configuration

## Tarefa 2
### Ponto a
gnu-rtr1 -> Vimos na linha hostname gnu-rtr1
### Ponto b
interface FastEthernet0/0
interface FastEthernet0/1
### Ponto c
#### interface FastEthernet0/0
172.16.254.45 255.255.255.0

#### interface FastEthernet0/1
172.16.30.1 255.255.255.0

### Ponto d
ip route 0.0.0.0 0.0.0.0 172.16.254.1
ip route 172.16.40.0 255.255.255.0 172.16.30.2

## Tarefa 3
### Ponto a
A interface FastEthernet0/1

### Ponto b
Estao disponiveis 14 endereços 7 de cada lista.

### Ponto c
O router está a usar overloading porque há um ip externo mapeado para vários internos


# Linux Routing

## Tarefa 1
default via 10.0.2.2 dev enp0s3 proto dhcp metric 100

## Tarefa 2
sudo ip route del default via 10.0.2.2 dev enp0s3

## Tarefa 3
traceroute -n 104.17.113.188
traceroute to 104.17.113.188 (104.17.113.188), 30 hops max, 60 byte packets
connect: Network is unreachable

## Tarefa 4
sudo ip route add 104.17.113.188 via 10.0.2.2 dev enp0s3

