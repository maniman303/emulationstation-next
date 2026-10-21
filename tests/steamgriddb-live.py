#!/usr/bin/env python3
"""Read-only API checks against the production client. Never persist the key.

Compile tests/steamgriddb-client.cpp with SteamGridDBClient.cpp first, then run:
    python3 tests/steamgriddb-live.py /tmp/steamgriddb-tests
"""
import getpass
import json
import subprocess
import sys
import urllib.error
import urllib.request

key = getpass.getpass("SteamGridDB API key (hidden): ")
binary = sys.argv[1]


def drive(name, credential, expected_error=0):
    child = subprocess.Popen([binary, "--drive", name], stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, text=True)
    pages = {}
    count = 0
    try:
        while True:
            line = child.stdout.readline().strip()
            if line.startswith("RESULT "):
                result = json.loads(line[7:])
                break
            assert line.startswith("REQUEST "), line
            request = json.loads(line[8:])
            path = request["path"]
            payload = json.loads(request["body"]) if request["body"] else None
            count += 1
            headers = {"Accept": "application/json", "User-Agent": "EmulationStation-SteamGridDB-test/1.0"}
            if credential and payload is None:
                headers["Authorization"] = "Bearer " + credential
            if payload is not None:
                headers["Content-Type"] = "application/json"
                headers["Referer"] = "https://www.steamgriddb.com/"
            try:
                with urllib.request.urlopen(urllib.request.Request(
                        "https://www.steamgriddb.com" + path, headers=headers,
                        data=json.dumps(payload).encode() if payload is not None else None), timeout=25) as response:
                    status, data = response.status, json.load(response)
            except urllib.error.HTTPError as error:
                status, data = error.code, {}
            except (urllib.error.URLError, TimeoutError):
                status, data = 3, {}
            if status == 200 and payload is not None:
                kind = 2 if payload["asset_type"] == "logo" else (0 if "460x215" in payload["dimensions"] else 1)
                pages.setdefault(kind, []).extend(data["data"]["assets"])
            child.stdin.write(str(status) + "\n" + json.dumps(data) + "\n")
            child.stdin.flush()
        assert child.wait(timeout=10) == 0
        assert result["error"] == expected_error, result
        if result["games"]:
            for kind, artwork in enumerate(result["games"][0]["artwork"]):
                if not pages.get(kind):
                    continue
                # Independent verification against every fetched API page.
                winner = max(pages[kind], key=lambda art: (
                    art["hearts"], art["width"] * art["height"], -art["id"]))
                assert artwork["id"] == winner["id"], (artwork, winner)
                assert artwork["url"].startswith("https://")
                # Read only the header to verify CDN downloads without saving files.
                with urllib.request.urlopen(urllib.request.Request(artwork["url"], headers={
                        "User-Agent": "Mozilla/5.0", "Range": "bytes=0-63"}), timeout=25) as image:
                    header = image.read(64)
                assert header.startswith(b"\x89PNG\r\n\x1a\n") or header.startswith(b"\xff\xd8"), "Invalid image header"
        print(name, "requests=" + str(count), json.dumps(result, ensure_ascii=False))
        return result
    finally:
        child.stdin.close()
        child.stdout.close()
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=10)


# Error::InvalidKey is 2; both missing Authorization and a wrong key must fail.
drive("Metroid Fusion", "", expected_error=2)
drive("Metroid Fusion", "0" * 32, expected_error=2)
assert drive("1633 - Metroid Fusion (USA) [!]", key)["games"][0]["name"] == "Metroid Fusion"
assert drive("0001 - Electroplankton (Japan)", key)["games"][0]["name"] == "Electroplankton"
assert "Emerald" in drive("Pokemon - Emerald Version (USA, Europe)", key)["games"][0]["name"]
assert not drive("asdkjasdzzzznonexistentgame87654", key)["games"]
print("Live authentication, filename search, pagination and artwork selection checks passed.")
