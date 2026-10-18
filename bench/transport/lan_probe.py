#!/usr/bin/env python3
"""B5 (phan LAN): do duong mang giua hai may truoc khi do SCTP/KCP.

Do RTT va jitter qua UDP, thong luong UDP mot chieu theo tung muc toc do (mat goi, doi thu tu),
va thong luong TCP mot luong, ca hai huong. Khong can cai gi them; chi dung thu vien chuan.

May chu:   python3 lan_probe.py server [--port 5201]
May khach: python3 lan_probe.py client <dia-chi-server> --machine <ten> [--port 5201]

Chay voi `python -I` neu thu muc hien tai khong tin cay.
"""
import argparse, json, socket, statistics, struct, sys, threading, time

HDR = struct.Struct("!cIQ")  # loai, id phien, so thu tu


def pace_send(sock, addr, sid, rate_mbps, secs, size):
    """Gui goi UDP theo toc do cho truoc; tra ve (so goi, so byte, giay thuc)."""
    payload = bytes(size - HDR.size)
    pps = rate_mbps * 1e6 / 8 / size
    start = time.perf_counter()
    end = start + secs
    seq = 0
    while True:
        now = time.perf_counter()
        if now >= end:
            break
        due = int((now - start) * pps)
        while seq < due:
            sock.sendto(HDR.pack(b"D", sid, seq) + payload, addr)
            seq += 1
        if seq >= due:
            time.sleep(0)  # nhuong GIL, khong ngu 15 ms nhu Windows
    return seq, seq * size, time.perf_counter() - start


class Counter:
    def __init__(self):
        self.n = self.bytes = self.reorder = 0
        self.max_seq = -1
        self.first = self.last = None
        self.lock = threading.Lock()

    def add(self, seq, nbytes):
        t = time.perf_counter()
        with self.lock:
            self.n += 1
            self.bytes += nbytes
            if seq < self.max_seq:
                self.reorder += 1
            self.max_seq = max(self.max_seq, seq)
            self.first = self.first or t
            self.last = t


# ---------------------------------------------------------------- may chu
def serve(port):
    counters = {}
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 << 20)
    udp.bind(("", port))
    peers = {}

    def udp_loop():
        while True:
            data, addr = udp.recvfrom(65535)
            if len(data) < HDR.size:
                continue
            kind, sid, seq = HDR.unpack_from(data)
            if kind == b"E":      # echo cho RTT
                udp.sendto(data, addr)
            elif kind == b"H":    # khach dang ky dia chi de nhan huong xuong
                peers[sid] = addr
            elif kind == b"D":
                counters.setdefault(sid, Counter()).add(seq, len(data))

    threading.Thread(target=udp_loop, daemon=True).start()

    def handle(conn):
        f = conn.makefile("rwb")
        cmd = json.loads(f.readline())
        c = cmd["cmd"]
        if c == "udp_up_report":
            ct = counters.pop(cmd["id"], Counter())
            span = (ct.last - ct.first) if ct.first and ct.last and ct.last > ct.first else 0
            out = dict(recv=ct.n, bytes=ct.bytes, max_seq=ct.max_seq, reorder=ct.reorder, span=span)
        elif c == "udp_down":
            addr = peers.get(cmd["id"])
            if not addr:
                out = dict(error="chua co dia chi khach")
            else:
                n, b, dt = pace_send(udp, addr, cmd["id"], cmd["rate"], cmd["secs"], cmd["size"])
                out = dict(sent=n, bytes=b, secs=dt)
        elif c == "tcp_down":   # server gui, khach nhan
            buf = bytes(1 << 16)
            end = time.perf_counter() + cmd["secs"]
            sent = 0
            while time.perf_counter() < end:
                conn.sendall(buf)
                sent += len(buf)
            conn.shutdown(socket.SHUT_WR)
            conn.recv(1)
            return
        elif c == "tcp_up":     # khach gui, server nhan
            total = 0
            while True:
                d = conn.recv(1 << 20)
                if not d:
                    break
                total += len(d)
            out = dict(bytes=total)
        else:
            out = dict(error="lenh la")
        f.write((json.dumps(out) + "\n").encode())
        f.flush()
        conn.close()

    tcp = socket.socket()
    tcp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    tcp.bind(("", port))
    tcp.listen(8)
    print(f"server san sang tren cong {port} (UDP+TCP)", flush=True)
    while True:
        conn, _ = tcp.accept()
        threading.Thread(target=handle, args=(conn,), daemon=True).start()


# ---------------------------------------------------------------- khach
def ctl(host, port, cmd):
    s = socket.create_connection((host, port), timeout=60)
    s.sendall((json.dumps(cmd) + "\n").encode())
    r = json.loads(s.makefile("rb").readline())
    s.close()
    return r


def rtt_test(udp, addr, n=300, gap=0.01):
    samples, lost = [], 0
    udp.settimeout(0.5)
    for i in range(n):
        t0 = time.perf_counter()
        udp.sendto(HDR.pack(b"E", 0, i), addr)
        try:
            while True:
                data, _ = udp.recvfrom(2048)
                if HDR.unpack_from(data)[2] == i:
                    samples.append((time.perf_counter() - t0) * 1000)
                    break
        except socket.timeout:
            lost += 1
        time.sleep(gap)
    if not samples:
        sys.exit("Mat het goi RTT: kiem tra tuong lua cua server (UDP) va dia chi")
    s = sorted(samples)
    jit = statistics.mean(abs(a - b) for a, b in zip(samples, samples[1:])) if len(samples) > 1 else 0
    return dict(min=s[0], p50=s[len(s) // 2], p99=s[int(len(s) * 0.99) - 1], max=s[-1], jitter=jit, lost=lost, n=n)


def client(a):
    addr = (a.host, a.port)
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 << 20)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 8 << 20)
    rows = []
    stamp = time.strftime("%Y%m%d")

    r = rtt_test(udp, addr)
    udp.settimeout(None)
    print(f"RTT   min {r['min']:.3f}  p50 {r['p50']:.3f}  p99 {r['p99']:.3f}  max {r['max']:.3f} ms  "
          f"jitter {r['jitter']:.3f}  mat {r['lost']}/{r['n']}")
    rows.append(("rtt", "", f"{r['p50']:.3f}", f"{r['p99']:.3f}", f"{r['jitter']:.3f}", r["lost"], "ms: p50,p99,jitter"))

    sid = int(time.time()) % 1_000_000 * 100  # tranh dem lan voi lan chay truoc con luu o server
    for rate in a.rates:
        sid += 1
        n, b, dt = pace_send(udp, addr, sid, rate, a.secs, a.size)
        time.sleep(0.3)
        rep = ctl(a.host, a.port, dict(cmd="udp_up_report", id=sid))
        loss = 100 * (1 - rep["recv"] / n) if n else 0
        got = rep["bytes"] * 8 / 1e6 / (rep["span"] or dt)
        print(f"UDP len  muc {rate:>5} Mbps: gui {b*8/1e6/dt:7.1f}, nhan {got:7.1f} Mbps, mat {loss:5.2f}%, doi thu tu {rep['reorder']}")
        rows.append(("udp-up", rate, f"{b*8/1e6/dt:.1f}", f"{got:.1f}", f"{loss:.2f}", rep["reorder"], "Mbps gui,nhan; % mat"))

    for rate in a.rates:
        sid += 1
        udp.sendto(HDR.pack(b"H", sid, 0), addr)
        time.sleep(0.1)
        ct = Counter()
        stop = threading.Event()

        def rx():
            udp.settimeout(0.2)
            while not stop.is_set():
                try:
                    d, _ = udp.recvfrom(65535)
                except socket.timeout:
                    continue
                k, s_, q = HDR.unpack_from(d)
                if k == b"D" and s_ == sid:
                    ct.add(q, len(d))

        t = threading.Thread(target=rx)
        t.start()
        rep = ctl(a.host, a.port, dict(cmd="udp_down", id=sid, rate=rate, secs=a.secs, size=a.size))
        time.sleep(0.3)
        stop.set()
        t.join()
        if "error" in rep:
            print("UDP xuong loi:", rep["error"])
            continue
        loss = 100 * (1 - ct.n / rep["sent"]) if rep["sent"] else 0
        span = (ct.last - ct.first) if ct.first and ct.last and ct.last > ct.first else rep["secs"]
        got = ct.bytes * 8 / 1e6 / span
        print(f"UDP xuong muc {rate:>5} Mbps: gui {rep['bytes']*8/1e6/rep['secs']:7.1f}, nhan {got:7.1f} Mbps, mat {loss:5.2f}%, doi thu tu {ct.reorder}")
        rows.append(("udp-down", rate, f"{rep['bytes']*8/1e6/rep['secs']:.1f}", f"{got:.1f}", f"{loss:.2f}", ct.reorder, "Mbps gui,nhan; % mat"))

    # TCP mot luong, ca hai huong
    s = socket.create_connection(addr)
    s.sendall((json.dumps(dict(cmd="tcp_up")) + "\n").encode())
    buf = bytes(1 << 16)
    t0 = time.perf_counter()
    end = t0 + a.secs
    tot = 0
    while time.perf_counter() < end:
        s.sendall(buf)
        tot += len(buf)
    s.shutdown(socket.SHUT_WR)
    s.makefile("rb").readline()
    mbps = tot * 8 / 1e6 / (time.perf_counter() - t0)
    s.close()
    print(f"TCP len   1 luong: {mbps:7.1f} Mbps")
    rows.append(("tcp-up", "", f"{mbps:.1f}", "", "", "", "Mbps"))

    s = socket.create_connection(addr)
    s.sendall((json.dumps(dict(cmd="tcp_down", secs=a.secs)) + "\n").encode())
    t0 = time.perf_counter()
    tot = 0
    while True:
        d = s.recv(1 << 20)
        if not d:
            break
        tot += len(d)
    mbps = tot * 8 / 1e6 / (time.perf_counter() - t0)
    s.sendall(b"x")
    s.close()
    print(f"TCP xuong 1 luong: {mbps:7.1f} Mbps")
    rows.append(("tcp-down", "", f"{mbps:.1f}", "", "", "", "Mbps"))

    import os
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "results"))
    os.makedirs(root, exist_ok=True)
    path = os.path.join(root, f"B5lan_{a.machine}_{stamp}.csv")
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"# server={a.host}; size={a.size}; secs={a.secs}; python={sys.version.split()[0]}\n")
        f.write("test,muc_mbps,v1,v2,v3,v4,ghi_chu\n")
        for r in rows:
            f.write(",".join(str(x) for x in r) + "\n")
    print("Ket qua:", path)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("mode", choices=["server", "client"])
    p.add_argument("host", nargs="?")
    p.add_argument("--port", type=int, default=5201)
    p.add_argument("--machine", default=socket.gethostname())
    p.add_argument("--secs", type=float, default=5)
    p.add_argument("--size", type=int, default=1200, help="byte moi goi UDP, ke ca header 13 byte")
    p.add_argument("--rates", type=int, nargs="+", default=[100, 300, 500, 800])
    a = p.parse_args()
    serve(a.port) if a.mode == "server" else client(a)
