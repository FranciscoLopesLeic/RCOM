# Experiência 1

Nesta experiência configuraram-se endereços IP de duas máquinas ligadas através de um switch para entender melhor como funciona a comuniação entre máquinas e o significado e funcionamento de pacotes ARP.

## O que são os pacotes ARP e para que são usados?
Os pacotes do protocolo ARP são utilizados para, sabendo o endereço IP de uma maquina, descobrir o seu endereço MAC. 

## O que são os endereços MAC e IP dos pacotes ARP e porquê?
Os endereços MAC identificam as interfaces com a rede que uma determinada máquina utiliza para comunicar com outras máquinas. Nos pacotes ARP utilizados para pedir o endereço MAC da interface de uma máquina, o primeiro endereço é enviado para que se identifique o endereço IP da máquina cujo endereço MAC é procurado. O segundo é utilizado para identificar a máquina que pretende ser informada. Nos pacotes ARP de resposta, o primeiro endereço IP é enviado para identificar a máquina que possui o endereço MAC enviado em segundo lugar.

## Que pacotes gera o comando PING?
São gerados pacotes ICMP utilizados para mandar erros da camada 3 ou mensagens para routers. Neste caso são utilizados para testar a conectividade entre as máquinas da rede.

## Quais são os endereços IP e MAC dos pacotes de PING?
O endereço IP de origem dos pacotes enviados é 172.16.50.1, o edereço do tux53. O endereço IP de destino é 172.16.50.254, o endereço de IP do tux54.  O endereço MAC de origem dos pacotes é 00:21:5a:61:2d:72 o endereço de eth0 do tux53. O endereço MAC de destino é 00:21:5a:c3:78:70, o endereço de eth0 do tux54.  

## Como determinar se um frame Ethernet recebido é ARP, IP, ICMP?
Ao analisar os pacotes no Wireshark pode verificar-se o tipo de frame analisando os bytes 12-13 do pacote recebido. Os pacotes ARP têm tipo 0x0806 os pacotes IP têm tipo 0x0800. Os pacotes ICMP têm o mesmo tipo dos pacotes IP e o byte 23 é 0x01. O Wireshark faz esta análise portanto basta ver a coluna Protocol para cada pacote.

## Como determinar o comprimento da trama recebida?
O comprimento das tramas pode ser observado no Wireshark na coluna Length. As tramas IP possuem o seu tamanho nos bytes 16-17.

# Experiência 2

Esta experiência teve como objetivo configurar duas VLAN's, bem como entender como funcionam domínios de broadcast

## Como configurar a vlan50
Primeiro conecta-se a interface eht0 do tux53 à conexão correspondente à porta 1 do switch e o a interface eth0 do tux54 à conexão correspondente à porta 2. Em seguida configuramos os endereços IP de ambas as interfaces para que correspondam à especificação

| Máquina | Interface | Endereço IP |
|---|---|---|
| Tux53 | eth0 | 172.16.50.1 |
| Tux54 | eth0 | 172.16.50.254 |

Em seguida criamos conectamos a porta série do Tux53 à porta série do switch e configuramos a VLAN50 com as portas 1 e 2 associadas à VLAN pois são as portas a que estão conectados os Tux53 e Tux54.

## Quantos dominios de broadcast há? Como o podemos cocluir tendo por base os logs?

Podemos concluir que há dois domínios de Broadcast. Um deles é na vlan50, uma vez que, os pacotes do ping do tux3 (172.16.50.1) atingem o tux4 (172.16.50.254), mas não atingem tux2(172.16.51.1) como se pode ver na figura (COLOCAR REFERENCIA A exp2-tsk8-tux3 e COLOCAR REFERENCIA A exp2-tsk8-tux4). O outro é na vlan51, no entanto, não há qualquer prova de Broadcast porque o tux2 se encontra isolado na vlan referida havendo apenas registo dos pacotes a saírem da origem como se pode ver na figura. (exp2-tsk10-tux4)

# Experiência 3

Esta experiência possibilitou o entendimento do sistema de DNS bem como a configuração de NAT num router comercial

## Como configurar uma rota estática num router comercial?

Executando o seguinte comando: 
    ip route 172.16.40.0 255.255.255.0 172.16.30.2

O primeiro endereço refere-se ao prefixo de origem dos pacotes, o segundo endereço é a máscara de sub-rede, e o último endereço é o gateway dos pacotes.

## Como configurar NAT num router comercial?
Para configurar a NAT deve-se seguir os seguintes comandos:
 - Identificar a interface de rede com o comando: interface FastEthernet0/0
 - Atribuir o endereço de IP que a interface vai tomar nessa NAT com: ip address 172.16.30.1 255.255.255.0
 - Especificar o tipo de NAT interno ou externo (neste caso externo): ip nat inside

Após configurar a interface deve configurar-se a pool de endereços exteriores disponiveis com: ip nat pool ovrld 172.16.254.45 172.16.254.45 prefix-length 24

Por fim, basta configurar a pool IP's internos:
 - ip nat inside source list 1 pool ovrld overload
 - access-list 1 permit 172.16.40.0 0.0.0.7
 - access-list 1 permit 172.16.30.0 0.0.0.7

## O que faz a NAT?
NAT é uma técnica que permite mapear uma gama de endereços IP para outra gama de endereços IP mudando o IP dos pacotes enviados enquanto estão em trânsito. É utilizada geralmente para mapear o endereço IP público fornecido por um ISP para o endereço privado da máquina do utilizador.

## Como configurar o serviço de DNS no host?
Pode-se configurar traduções especificas em /etc/hosts como fizemos com o mapeamento do endereço de youtubas. Pode-se configurar um servidor central que faz a tradução no ficheiro /etc/resolv.conf adicionando: nameserver {IP_DNS}

## Que pacotes são trocados pelo DNS e que informação é transportada
São observados dois tipos de pacote muito semelhantes. Ambos transportam o hostname sobre o qual se quer saber o IP. A resposta aos pacotes do tipo 'A' é um endereço IPv4, já a resposta ao pacotes AAAA é um endereço IPv6.

## Que pacotes ICMP são observados e porquê?
O traceroute tenta encontrar o número mínimo de saltos para alcançar um determinado destino. Para isso gera pacotes UDP com TTL crescente partindo de 1. Quando recebe um pacote ICPM que informa que TTL foi excedido o traceroute sabe que necessita de pelo menos mais um valor de TTL para alcançar o destino. No nosso caso como o destino não aceita pedidos é ainda enviado um pacote ICPM que informa que a porta é inalcançável.

## Quais são os endereços IP e MAC associados a pacotes ICMP e porquê?
Os endereços IP de destino são sempre da nossa máquina, os de origem correspondem aos vários IP intermédios pelos quais os pacotes devem viajar até alcançar o destino. Cada vez que o traceroute aumenta o TTL, o endereço de origem muda, o que significa que o pacote atingiu mais um nó. O endereço MAC de origem é sempre o endereço MAC da interface virtual do computador host, o o endereço de destino é o MAC da interface virtual do Guest OS.

## Quais são as rotas na sua máquina? Qual o seu significado?
A rota com origem em 0.0.0.0 e  destino em 172.24.64.1 é a default gateway é indica por onde devem ser enviados os pacotes caso não possam ser encaminhados para a rede local.
A rota com origem em 172.24.64.0 e gateway 0.0.0.0 significa que todos os pacotes com endereço de destino que sejam compativeis com 172.14.64.0/24 não têm gateway e devem ser encaminhados localmente.

# Experiência 4

Esta experiência é a junção do conhecimento das experiências anteriores para a formação de uma rede interna com duas VLAN's um computador que serve de ponte entre elas e a ligação à internet utilizando o sistema de NAT de um router comercial. O trabalho desenvolvido nesta experiência foi feito fora das aulas teórico-práticas na bancada 1.

## Que rotas há nos tuxes? Qual é o seu significado?

### Tux12
| Destino | Gateway | Máscara de subrede |
|---|---|---|
| 0.0.0.0 | 172.16.11.254 | 0.0.0.0 |
| 172.16.11.0 | 0.0.0.0 | 255.255.255.0 |
| 172.16.10.0 | 172.16.11.253 | 255.255.255.0 |

- Primeira entrada: é o endereço default, indica que todos os pacotes que não tenham match em qualquer outra rota devem ser enviados para o endereço IP 172.16.11.254 neste caso o endereço do router, que encaminhará o pacote para a internet.
- Segunda entrada: significa que qualquer pacote que tenha como destino a VLAN11 deve ser tratado localmente (0.0.0.0) uma vez que já atingiu a VLAN correta e não tem de ser encaminhado.
- Terceira entrada: significa que qualquer pacote que tenha como destino a VLAN10 deve ser enviado para o endereço 172.16.11.253 o endereço da interface do tux14 na VLAN11, uma vez que este computador é a interface entre as duas VLAN e saberá encaminhar o pacote corretamente.

### Tux13
| Destino | Gateway | Máscara de subrede |
|---|---|---|
| 0.0.0.0 | 172.16.10.254 | 0.0.0.0 |
| 172.16.10.0 | 0.0.0.0 | 255.255.255.0 |
| 172.16.11.0 | 172.16.10.254 | 255.255.255.0 |

- Primeira entrada: é o endereço default, indica que todos os pacotes que não tenham match em qualquer outra rota devem ser enviados para o endereço IP 172.16.10.254 neste caso o endereço do tux14, que encaminhará o pacote para o router.
- Segunda entrada: significa que qualquer pacote que tenha como destino a VLAN10 deve ser tratado localmente (0.0.0.0) uma vez que já atingiu a VLAN correta e não tem de ser encaminhado.
- Terceira entrada: significa que qualquer pacote que tenha como destino a VLAN11 deve ser enviado para o endereço 172.16.10.254 o endereço da interface do tux14 na VLAN10, uma vez que este computador é a interface entre as duas VLAN e saberá encaminhar o pacote corretamente.

### Tux14
| Destino | Gateway | Máscara de subrede |
|---|---|---|
| 0.0.0.0 | 172.16.11.254 | 0.0.0.0 |
| 172.16.10.0 | 0.0.0.0 | 255.255.255.0 |
| 172.16.11.0 | 0.0.0.0 | 255.255.255.0 |

- Primeira entrada: é o endereço default, indica que todos os pacotes que não tenham match em qualquer outra rota devem ser enviados para o endereço IP 172.16.11.254 neste caso o endereço do router, que encaminhará o pacote para a internet.
- Segunda entrada: significa que qualquer pacote que tenha como destino a VLAN10 deve ser tratado localmente (0.0.0.0) uma vez que já atingiu a VLAN correta e não tem de ser encaminhado.
- Terceira entrada: significa que qualquer pacote que tenha como destino a VLAN11 deve ser tratado localmente (0.0.0.0) uma vez que já atingiu a VLAN correta e não tem de ser encaminhado.

As duas ultimas entradas têm o mesmo significado uma vez que o tux14 se encontra conectado às duas VLAN e conhece todos os endereços das mesmas logo saberá encaminhar pacotes em ambos os casos.

## Que informação contém uma entrada da fowarding table?

Uma entrada na fowarding table contém os seguintes dados:
    - Endereço de destino para uma subrede (Destination) 
    - Máscara de subrede(Genmask)
    - Endereço do próximo salto (Gateway)
    - Flags associadas com ACL
    - Métrica associada à ligação. É escolhida a entrada com a métrica menor
    - Número de rotas que se referem aquela entrada, este valor não é usado pelo kernel Linux (Ref)
    - Coluna com número de consultas à rota (Use)
    - Interface de comunicação (Iface)

## Que mensagens ARP, e endereços MAC, são observados e porquê?
Ao fazer ping do tux13 para o tux12 capturando pacotes nas duas interfaces são observados os seguintes pacotes ARP.
Primeiro é observado um pacote que pede que o endereço MAC do IP 172.16.10.254 seja envidado para 172.16.10.1, o endereço MAC observado é 00:21:5a:61:2f:24. isto acontece porque o tux13 está a encontrar a interface do tux14 que encaminhará o pacote enviado pelo ping para o tux12, o endereço MAC de resposta corresponde à interface eth0 de tux14.
Em seguida na mesma interface é observado o pacote ARP que pede que o endereço MAC de 172.16.10.1 seja enviado para 172.16.10.254, isto acontece porque o tux14 necessita do endereço MAC da interface do tux13 para encaminhar a resposta do tux12 ao ping. O endereço MAC observado é 00:21:5a:61:2d:ef o endereço de eth0 do tux13.

Já em eth1 do tux14 acontece algo semelhante, o tux14 pergunta qual o endereço MAC do tux12 para lhe poder encaminhar o pacote de PING e este responde com 00:21:5a:61:2e:c3, o endereço MAC de eth0 do tux12. Em seguida o tux12 pede o endereço MAC de eth1 de tux14 para lhe poder enviar a resposta a PING para que esta seja encaminhada a tux13. O tux14 responde com 00:c0:df:04:20:99 o endereço MAC de eth1 do tux14.

## Quais são os endereços IP e MAC associados aos pacotes ICMP e porquê?
Quando o tux13 envia o pacote ICMP para o tux12 o endereço IP de origem é 172.16.10.1 o endereço de tux13 e o de destino 172.16.11.1, o endereço de tux12. O endereço MAC de origem é o endereço 00:21:5a:61:2d:ef o enderço da interface de tux13 e o de destino 00:21:5a:61:2f:24, o endereço MAC de eth0 de tux14. Isto acontece porque o endereço MAC de destino é sempre o endereço da próxima interface e não o endereço MAC correspondente ao destino final. Como os pacotes têm de passar por tux4 este é o endereço MAC de destino. O mesmo acontece com os pacotes de resposta, os endereços IP são os endereços da origem neste caso o tux12 e o de destino é o de tux13. Já o endereço MAC de origem é o endereço de eth0 de tux14 e o de destino eth0 de tux13.

Na interface eth1 de tux4 acontece algo semelhante. Os pacotes enviados por tux13 têm o seu endereço IP como endereço IP de origem e o endereço IP de tux12 como destino. Já o endereço MAC de origem é o de eth1 de tux14 e o de destino o de eth0 de tux12. O contrário acontece com os pacotes de resposta, estes possuem como endereço de IP de origem o endereço de tux12 e o de destino o de tux13. Mas o endereço MAC de origem é eth0 de tux12 e o de destino eth1 de tux14.

## Quais são os caminhos seguidos pelos pacotes nas experiências realizadas e porquê?

### Router CISCO para tux12
O caminho seguido é router > tux12 pois estes encontram-se na mesma VLAN e podem comunicar diretamente

### Router CISCO para tux13
O caminho seguido é router > tux14 > tux13 pois o router encontra-se na VLAN11 e o tux13 na VLAN10 por isso os pacotes enviados têm de ser enviados primeiro para tux14 

### Router CISCO para tux14
A comunicação é direta uma vez que ambos têm interfaces conectadas à mesma VLAN

### Router CISCO para a 172.16.1.254
A comunicação pode ser feita diretamente pois o router têm uma interface ligada à VLAN onde está a máquina de endereço 172.16.1.254.

### Tux13 para 172.16.1.254
O caminho seguido é tux13 > tux14 > router > 172.16.1.254, isto acontece porque o tux13 não tem ligação direta a 172.16.1.254 nem ao router por isso o pacote tem de ser enviado ao tux14 que o encaminhará para o router. O router por sua vez encaminha o pacote para 172.16.1.254.

### Tux13 para 104.17.113.188
O caminho seguido é  tux13 > tux14 > router > 172.16.1.254 > ... > 104.17.113.188, a justificação para o encaminhamento até 172.16.1.254 é a mesma que na comunicação anterior, no entanto após atingir 172.16.1.254 o pacote será encaminhado para a Internet, aí fará um caminho que desconhecemos até atingir 104.17.113.188. 
