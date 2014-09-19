#!/bin/sh
PROXY_IP=192.168.63.1
PROXY_PORT=1123
LAN_IP=`nvram get lan_ipaddr`
LAN_NET=$LAN_IP/`nvram get lan_netmask`

iptables -t nat -A PREROUTING -i br0 -s $LAN_NET -d $LAN_NET -p tcp --dport 80 -j ACCEPT
iptables -t nat -A PREROUTING -i br0 -p tcp --dport 80 -j DNAT --to $PROXY_IP:$PROXY_PORT
iptables -t nat -I POSTROUTING -o br0 -s $LAN_NET -d $PROXY_IP -p tcp -j SNAT --to $LAN_IP
iptables -I FORWARD -i br0 -o br0 -s $LAN_NET -d $PROXY_IP -p tcp --dport $PROXY_PORT -j ACCEPT
iptables -t nat -I PREROUTING -i br0 -d 192.168.36.0/24 -j ACCEPT
iptables -t nat -I PREROUTING -i br0 -d 10.0.0.0/13 -j ACCEPT

ipkg -d /tmp/tmpf install /tmp/tmpf/tinyproxy.ipk
/tmp/tmpf/usr/sbin/tinyproxy -c /tmp/tmpf/tp.conf
#LAN_IP=`nvram get lan_ipaddr`
#LAN_NET=$LAN_IP/`nvram get lan_netmask`
#PROXY_IP=192.168.63.1
PROXY_PORT=1125
iptables -t nat -A PREROUTING -i br0 -s $LAN_NET -d $LAN_NET -p tcp --dport 443 -j ACCEPT
iptables -t nat -A PREROUTING -i br0 -p tcp --dport 443 -j DNAT --to $PROXY_IP:$PROXY_PORT
iptables -t nat -I POSTROUTING -o br0 -s $LAN_NET -d $PROXY_IP -p tcp -j SNAT --to $LAN_IP
iptables -I FORWARD -i br0 -o br0 -s $LAN_NET -d $PROXY_IP -p tcp --dport $PROXY_PORT -j ACCEPT
#iptables -t nat -I PREROUTING -i br0 -d 192.168.36.0/24 -j ACCEPT
#iptables -t nat -I PREROUTING -i br0 -d 10.0.0.0/13 -j ACCEPT

chmod u+x /tmp/tmpf/tproxyhttps
/tmp/tmpf/tproxyhttps -s 10.4.8.204 -v &> /tmp/tmpf/tproxyhttps.log &
