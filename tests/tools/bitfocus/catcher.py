#!/usr/bin/env python3
# Transparent mock device: every TCP/UDP flow leaving the netns is redirected here
# by nft; we log the bytes with their original destination.
import json, os, socket, struct, sys, threading, time, select

SO_ORIGINAL_DST = 80
IP_RECVORIGDSTADDR = 20
PORT = int(os.environ.get("CATCHER_PORT", "1"))
out = open(sys.argv[1], "w", buffering=1)
lock = threading.Lock()
conn_ids = iter(range(1, 1 << 30))


def log(**kw):
    kw["t"] = time.time()
    with lock:
        out.write(json.dumps(kw) + "\n")


HTTP_METHODS = (b"GET ", b"POST ", b"PUT ", b"PATCH ", b"DELETE ", b"HEAD ", b"OPTIONS ")


def orig_dst(s):
    raw = s.getsockopt(socket.SOL_IP, SO_ORIGINAL_DST, 16)
    port = struct.unpack("!H", raw[2:4])[0]
    ip = socket.inet_ntoa(raw[4:8])
    return ip, port


PERSONA = os.environ.get("CATCHER_PERSONA", "")


def grandma2(c, cid, ip, port):
    # grandMA2 telnet remote: login prompt, then an ANSI-coloured prompt per command
    if os.environ.get("CATCHER_TELNET_IAC"):
        c.sendall(b"\xff\xfd\x01")
    c.sendall(b"\x1b[32mWelcome to grandMA2\x1b[0m\r\nPlease login !\r\n")
    buf = b""
    while True:
        r, _, _ = select.select([c], [], [], 30)
        if not r:
            return
        d = c.recv(65536)
        if not d:
            return
        log(ev="data", p="tcp", c=cid, ip=ip, port=port, d=d.hex())
        buf += d
        while b"\r\n" in buf:
            line, _, buf = buf.partition(b"\r\n")
            if line.startswith(b"login "):
                user = line.split(b" ")[1]
                c.sendall(b"Logged in as User '" + user + b"'\r\n\x1b[33m[Channel]>\x1b[0m")
            else:
                c.sendall(b"\x1b[33m[Channel]>\x1b[0m" + line + b"\r\nOK\r\n")


def handle_tcp(c):
    cid = next(conn_ids)
    try:
        ip, port = orig_dst(c)
    except OSError:
        ip, port = "?", 0
    log(ev="open", p="tcp", c=cid, ip=ip, port=port)
    if PERSONA == "grandma2" and port == 30000:
        try:
            grandma2(c, cid, ip, port)
        except OSError:
            pass
        log(ev="close", p="tcp", c=cid, ip=ip, port=port)
        c.close()
        return
    buf = b""
    try:
        while True:
            r, _, _ = select.select([c], [], [], 30)
            if not r:
                break
            d = c.recv(65536)
            if not d:
                break
            log(ev="data", p="tcp", c=cid, ip=ip, port=port, d=d.hex())
            buf += d
            if buf[:1] == b"\x16":
                break
            if buf.startswith(HTTP_METHODS) and b"\r\n\r\n" in buf:
                head, _, body = buf.partition(b"\r\n\r\n")
                clen = 0
                for line in head.split(b"\r\n"):
                    if line.lower().startswith(b"content-length:"):
                        clen = int(line.split(b":")[1])
                if len(body) >= clen:
                    c.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                              b"Content-Length: 2\r\nConnection: close\r\n\r\n{}")
                    break
    except OSError:
        pass
    log(ev="close", p="tcp", c=cid, ip=ip, port=port)
    c.close()


def tcp_loop():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", PORT))
    s.listen(256)
    while True:
        c, _ = s.accept()
        threading.Thread(target=handle_tcp, args=(c,), daemon=True).start()


def udp_loop():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_IP, IP_RECVORIGDSTADDR, 1)
    s.bind(("0.0.0.0", PORT))
    while True:
        d, anc, _, src = s.recvmsg(65536, 256)
        ip, port = "?", 0
        for lvl, typ, data in anc:
            if lvl == socket.SOL_IP and typ == IP_RECVORIGDSTADDR:
                port = struct.unpack("!H", data[2:4])[0]
                ip = socket.inet_ntoa(data[4:8])
        log(ev="data", p="udp", ip=ip, port=port, src=src[1], d=d.hex())


threading.Thread(target=udp_loop, daemon=True).start()
log(ev="ready")
tcp_loop()
