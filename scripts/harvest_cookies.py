#!/usr/bin/env python3
"""Harvest CF cookies from the daemon's CDP browser into a plugin jar.
Usage: harvest_cookies.py <cdp_port> <domain> <out.jar>
One-shot seed; the daemon refreshes the jar on later curl successes."""
import json, sys, urllib.request
import websocket  # websocket-client

port, domain, out = sys.argv[1], sys.argv[2], sys.argv[3]
port = port.split(":")[-1]
targets = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json"))
page = next((t for t in targets if t.get("type") == "page"), None)
if page is None:
    sys.exit("no page target")
# browser-level cookies (httpOnly included); page-session Network.getAllCookies
# sees more (partitioned jars) - try page-level first, browser-level fallback
page_ws = page["webSocketDebuggerUrl"]
ws = websocket.create_connection(page_ws, timeout=15)

mid = 0
def cmd(method, params=None):
    global mid
    mid += 1
    ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
    while True:
        msg = json.loads(ws.recv())
        if msg.get("id") == mid:
            return msg.get("result", {})

def cookies_via(method):
    try:
        res = cmd(method, {})
        return res.get("cookies", [])
    except Exception:
        return []

cookies = cookies_via("Network.getAllCookies") or cookies_via("Storage.getCookies")
rows = []
for c in cookies:
    d = c.get("domain", "")
    if domain in d or d.lstrip(".") in domain:
        rows.append("\t".join([
            d, "TRUE", c.get("path", "/"), "TRUE" if c.get("secure") else "FALSE",
            str(c.get("expires", 2147483647)), c.get("name", ""), c.get("value", ""),
        ]))
with open(out, "w") as f:
    f.write("# Netscape HTTP Cookie File\n" + "\n".join(r for r in rows if r.strip()) + "\n")
print(f"{len(rows)} cookies for {domain} -> {out}")
for r in rows:
    print(" ", r.split("\t")[5], len(r.split("\t")[6]))
