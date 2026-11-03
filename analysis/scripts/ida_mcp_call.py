#!/usr/bin/env python3
"""
Drive a running "ida" MCP server (streamable HTTP, e.g. the zeromcp/ida-mcp
plugin at http://127.0.0.1:8737/mcp) from the command line.

Usage:

    python3 ida_mcp_call.py snippet.py            # execute IDAPython file
    python3 ida_mcp_call.py --list-tools          # show available tools

The snippet's last expression (or a run(db)/main(db) function) is printed.
`db` is the ida-domain Database of the currently open binary; classic
IDAPython modules (idc, idaapi, idautils) also work.

This is plain JSON-RPC over HTTP with urllib - no dependencies beyond
Python 3. Keep a session alive only as long as you need it.
"""

import argparse
import json
import sys
import urllib.request

MCP_URL = "http://127.0.0.1:8737/mcp"


class McpClient:
    def __init__(self, url=MCP_URL):
        self.url = url
        self.session = None

    def _post(self, payload):
        headers = {"Content-Type": "application/json",
                   "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        req = urllib.request.Request(self.url, data=json.dumps(payload).encode(),
                                     headers=headers)
        with urllib.request.urlopen(req, timeout=600) as resp:
            if not self.session:
                self.session = resp.headers.get("Mcp-Session-Id")
            body = resp.read().decode()
        if not body:
            return None  # e.g. 204 for notifications
        for line in body.splitlines():  # tolerate text/event-stream framing
            if line.startswith("data:"):
                body = line[len("data:"):].strip()
                break
        return json.loads(body)

    def initialize(self):
        r = self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                        "params": {"protocolVersion": "2024-11-05",
                                   "capabilities": {},
                                   "clientInfo": {"name": "ida_mcp_call", "version": "1.0"}}})
        # required notification before any other call
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        return r.get("result", {}).get("serverInfo", {})

    def list_tools(self):
        r = self._post({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        return [t["name"] for t in r["result"]["tools"]]

    def execute_python(self, code, instance_id=None):
        args = {"code": code}
        if instance_id:
            args["instance_id"] = instance_id
        r = self._post({"jsonrpc": "2.0", "id": 3, "method": "tools/call",
                        "params": {"name": "execute_python", "arguments": args}})
        result = r.get("result", {})
        if result.get("isError"):
            return "ERROR: " + result["content"][0]["text"]
        return result["content"][0]["text"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snippet", nargs="?", help="Python file to execute in IDA")
    parser.add_argument("--list-tools", action="store_true")
    parser.add_argument("--url", default=MCP_URL)
    args = parser.parse_args()

    if not args.snippet and not args.list_tools:
        parser.error("give a snippet file or --list-tools")

    client = McpClient(args.url)
    info = client.initialize()
    print("# server: %s %s" % (info.get("name"), info.get("version")), file=sys.stderr)

    if args.list_tools:
        for name in client.list_tools():
            print(name)
        return

    code = open(args.snippet).read()
    print(client.execute_python(code))


if __name__ == "__main__":
    main()
