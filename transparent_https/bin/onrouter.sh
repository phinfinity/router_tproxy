# iptables commands to be run on DD-WRT for tproxyhttps intercepting
# Note your router setup may differ , use only as a guide
LAN_IP=`nvram get lan_ipaddr`
LAN_NET=$LAN_IP/`nvram get lan_netmask`
PROXY_IP=192.168.128.1 # IP address of router
PROXY_PORT=1125 # These two are refering to the intermediate proxy (tproxyhttps)

iptables -t nat -A PREROUTING -i br0 -s $LAN_NET -d $LAN_NET -p tcp --dport 443 -j ACCEPT
iptables -t nat -A PREROUTING -i br0 -p tcp --dport 443 -j DNAT --to $PROXY_IP:$PROXY_PORT
iptables -t nat -I POSTROUTING -o br0 -s $LAN_NET -d $PROXY_IP -p tcp -j SNAT --to $LAN_IP
iptables -I FORWARD -i br0 -o br0 -s $LAN_NET -d $PROXY_IP -p tcp --dport $PROXY_PORT -j ACCEPT
iptables -t nat -I PREROUTING -i br0 -d 192.168.36.0/24 -j ACCEPT
iptables -t nat -I PREROUTING -i br0 -d 10.0.0.0/13 -j ACCEPT

/tmp/tproxyhttps -s proxy.iiit.ac.in -a 8080 -p $PORT -v &> /tmp/tproxyhttps.log &

