#!/usr/bin/env python3
"""Mint a fresh readcomicsonline Cloudflare cookie from the daemon's Chrome.

The daemon owns a real (headed-under-Xvfb) Chrome with a non-headless UA; it
clears the site's JS challenge automatically, unlike curl. This script opens a
throwaway tab in that browser, waits for the challenge to resolve, copies the
bound Cookie header + User-Agent into the plugin's gitignored config.json, and
closes the tab. Requires `websocket-client` (already installed on the host).

    python3 scripts/refresh-rco-cookie.py     # then POST /plugins/reload or restart
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
import urllib.request

try:
    import websocket  # websocket-client
except ImportError:
    sys.exit("need websocket-client: pip install websocket-client")

SITE = "https://readcomicsonline.ru"
PROBE = SITE + "/advanced-search?name=batman"
PLUGIN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      "plugins_dev", "lua", "readcomiconline", "config.json")
KEEP = {"cf_clearance", "read-comics-online-session", "XSRF-TOKEN"}


def debug_port() -> int:
    ports = set(re.findall(r"--remote-debugging-port=(\d+)",
                           subprocess.check_output(["ps", "-eo", "args"], text=True)))
    for p in ports:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{p}/json/version", timeout=2) as r:
                if json.load(r).get("webSocketDebuggerUrl"):
                    return int(p)
        except Exception:
            continue
    sys.exit("daemon Chrome not found -- start cppIsekai first (run-cppIsekai.sh)")


def main() -> None:
    port = debug_port()
    req = urllib.request.Request(f"http://127.0.0.1:{port}/json/new?{PROBE}", method="PUT")
    with urllib.request.urlopen(req, timeout=15) as r:
        tab = json.load(r)
    ws = websocket.create_connection(tab["webSocketDebuggerUrl"], timeout=30)
    seq = 0

    def cmd(method: str, params: dict | None = None) -> dict:
        nonlocal seq
        seq += 1
        ws.send(json.dumps({"id": seq, "method": method, "params": params or {}}))
        while True:
            msg = json.loads(ws.recv())
            if msg.get("id") == seq:
                return msg

    try:
        cmd("Network.enable")
        deadline = time.monotonic() + 75
        title = ""
        while time.monotonic() < deadline:
            r = cmd("Runtime.evaluate", {
                "expression": "JSON.stringify({t:document.title,l:document.body?document.body.innerText.length:0})",
                "returnByValue": True,
            })
            try:
                state = json.loads(r["result"]["result"]["value"])
            except (KeyError, TypeError, ValueError):
                state = {}
            title = state.get("t", "")
            if title and "Just a moment" not in title and state.get("l", 0) > 500:
                break
            time.sleep(1)
        if "Just a moment" in title or not title:
            sys.exit(f"Cloudflare challenge did not clear in 75s (title={title!r})")

        ua = cmd("Runtime.evaluate", {"expression": "navigator.userAgent", "returnByValue": True})
        agent = ua["result"]["result"]["value"]
        cookies = cmd("Network.getCookies", {"urls": [SITE + "/"]}).get("result", {}).get("cookies", [])
        picked = {c["name"]: c["value"] for c in cookies if c["name"] in KEEP}
        if "cf_clearance" not in picked:
            sys.exit("challenge cleared but no cf_clearance cookie in the jar")
        cookie = "; ".join(f"{k}={v}" for k, v in picked.items())
    finally:
        ws.close()
        try:
            urllib.request.urlopen(f"http://127.0.0.1:{port}/json/close/{tab['id']}", timeout=5).close()
        except Exception:
            pass

    old = ""
    try:
        with open(PLUGIN, encoding="utf-8") as f:
            old = json.load(f).get("cookie", "")
    except (OSError, ValueError):
        pass
    if cookie == old:
        print("cookie unchanged")
        return
    with open(PLUGIN, "w", encoding="utf-8") as f:
        json.dump({"user_agent": agent, "cookie": cookie}, f, indent=2)
        f.write("\n")
    os.chmod(PLUGIN, 0o600)
    print(f"wrote {PLUGIN}: cf_clearance {len(picked['cf_clearance'])} chars, "
          f"UA Chrome {re.search(r'Chrome/([\d.]+)', agent).group(1) if re.search(r'Chrome/([\d.]+)', agent) else '?'}")


if __name__ == "__main__":
    main()
