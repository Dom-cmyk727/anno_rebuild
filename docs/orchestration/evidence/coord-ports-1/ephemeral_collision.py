"""Negative control: a kernel-assigned client port blocks a coordinator bind.

Opens a listener, connects a client WITHOUT bind (the kernel picks the local port
from ip_local_port_range), then starts anno_budget_coord on that same local port:
(1) while the client is connected, (2) after the client closed first (its socket
in TIME_WAIT), (3) for a second client whose PEER closed first (no TIME_WAIT on
the client's port). Prints `ss` for the port each time.
"""
import os, socket, subprocess, sys, tempfile, time

coord = sys.argv[1]
journal_dir = tempfile.mkdtemp()
print("ip_local_port_range:", open("/proc/sys/net/ipv4/ip_local_port_range").read().split())

STATES = {"01": "ESTABLISHED", "06": "TIME_WAIT", "0A": "LISTEN", "08": "CLOSE_WAIT",
          "09": "LAST_ACK", "07": "CLOSE", "04": "FIN_WAIT1", "05": "FIN_WAIT2"}

def ss(port):
    """Sockets on `port` from /proc/net/tcp (local:remote state)."""
    rows = []
    for line in open("/proc/net/tcp").read().splitlines()[1:]:
        f = line.split()
        lp, rp = int(f[1].split(":")[1], 16), int(f[2].split(":")[1], 16)
        if port in (lp, rp):
            rows.append(f"{lp}->{rp} {STATES.get(f[3], f[3])}")
    return " | ".join(rows) or "(none)"

def coordinator(port):
    p = subprocess.Popen([coord, str(port), "local", "--journal",
                          os.path.join(journal_dir, f"{port}.journal")],
                         stdout=subprocess.PIPE, text=True)
    line = p.stdout.readline().strip()
    p.kill(); p.wait()
    return line.split(" build=")[0]

def pair():
    srv = socket.socket(); srv.bind(("127.0.0.1", 0)); srv.listen(1)
    cli = socket.socket(); cli.connect(srv.getsockname()); peer, _ = srv.accept()
    return srv, cli, peer, cli.getsockname()[1]

results = {}
srv, cli, peer, port = pair()
print(f"A: client local port (kernel-assigned) {port}; sockets: {ss(port)}")
results["A connected"] = coordinator(port)
cli.close(); time.sleep(0.2)
print(f"   after client closed first: {ss(port)}")
results["A client closed first"] = coordinator(port)
peer.close(); srv.close()

srv, cli, peer, port2 = pair()
peer.close(); time.sleep(0.2); cli.close(); time.sleep(0.2)
print(f"B: client local port {port2}; peer closed first, then client: {ss(port2)}")
results["B peer closed first"] = coordinator(port2)
srv.close()
for k, v in results.items():
    print(f"{k:24s} -> {v}")
ok = results["A connected"].startswith("BIND-FAILED")
print("RESULT", "reproduced: a live kernel-assigned client socket blocks the bind" if ok else "NOT reproduced")
sys.exit(0 if ok else 1)
