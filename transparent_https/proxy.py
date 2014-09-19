#!/usr/bin/env python2
#Author Phinfinity <rndanish@gmail.com>
"""
Simple hacked up https transparent proxy
works on desktops/laptops running linux
Add iptables rules (in order):
iptables -t nat -A OUTPUT -d 192.168.0.0/16 -j ACCEPT
iptables -t nat -A OUTPUT -d 10.0.0.0/8 -j ACCEPT
iptables -t nat -A OUTPUT -d 172.16.0.0/12 -j ACCEPT
iptables -t nat -A OUTPUT -d 127.0.0.1 -j ACCEPT
iptables -t nat -A OUTPUT -p tcp --dport 443 -j DNAT --to 127.0.0.1:1234
"""
import logging
import threading
import os
import signal
from socket import *
import struct
logging.basicConfig()
logger = logging.getLogger("proxy")
logger.setLevel(logging.DEBUG)
SO_ORIGINAL_DST = 80
PROXY_HOST = "proxy.iiit.ac.in"
PROXY_PORT = 8080
SERVER_HOST = "0.0.0.0"
SERVER_PORT = 1234
SERVER_CONN_BUFFER = 20

def pipe_data(s_from, s_to):
    try:
        while True:
            data = s_from.recv(2048)
            if len(data) == 0:
                return
            s_to.send(data)
    finally:
        try:
            s_from.shutdown(SHUT_RDWR)
            s_from.close()
        except error:
            pass
        try:
            s_to.shutdown(SHUT_RDWR)
            s_to.close()
        except error:
            pass

def wrap_https_proxy(proxy_s, s, dest_host, dest_port):
    logger.info("Wrapping HTTPS connection on proxy")
    https_conn = "CONNECT {host}:{port} HTTP/1.1\r\nHost: {host}\r\n\r\n"
    https_conn = https_conn.format(host=dest_host, port=dest_port)
    proxy_s.send(https_conn)
    data = proxy_s.recv(1024)
    if not data.startswith("HTTP/1.0 200"):
        logger.info("Wrapped connection failed")
        logger.info("Got : %s", data.split('\n')[0])
        proxy_s.close()
        s.close()
    else:
        logger.info("Wrapped connection looks fine")


def handle_connection(s):
    try:
        proxy_s = socket(AF_INET, SOCK_STREAM)
        proxy_s.connect((PROXY_HOST, PROXY_PORT))
        dst = conn.getsockopt(SOL_IP, SO_ORIGINAL_DST, 16)
        srv_port, srv_ip = struct.unpack("!2xH4s8x", dst)
        srv_host = inet_ntoa(srv_ip)
        logger.info("Intercepted connection to %s:%d", srv_host, srv_port)
        wrap_https_proxy(proxy_s, s, srv_host, srv_port)
        t1 = threading.Thread(target=pipe_data, args=(s, proxy_s))
        t2 = threading.Thread(target=pipe_data, args=(proxy_s, s))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
        logger.info("%s:%d Terminated", srv_host, srv_port)
    finally:
        try:
            proxy_s.close()
        except error:
            pass
        try:
            s.close()
        except error:
            pass


pid = os.getpid()
s = socket(AF_INET, SOCK_STREAM)
s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
s.bind((SERVER_HOST, SERVER_PORT))
s.listen(SERVER_CONN_BUFFER)
logger.info("pid:%d , Listening on %d", pid, SERVER_PORT)
while True:
    try:
        conn, addr = s.accept()
        threading.Thread(target=handle_connection, args=(conn,)).start()
    except KeyboardInterrupt:
        os.kill(pid, signal.SIGKILL)
