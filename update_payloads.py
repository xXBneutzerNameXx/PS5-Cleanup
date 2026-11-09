#!/usr/bin/env python3
"""Schreibt payloads.json neu aus dem neuesten Release von PS5-Cleanup."""
import hashlib, json, os, re, sys, urllib.request

REPO = "xXBneutzerNameXx/PS5-Cleanup"
ASSET = re.compile(r"^ps5cleanup_v[\w.\-]+\.elf$", re.I)
OUT = "payloads.json"


def get(url, accept="application/vnd.github+json"):
    req = urllib.request.Request(url, headers={"Accept": accept, "User-Agent": "pm-source-updater"})
    tok = os.environ.get("GITHUB_TOKEN")
    if tok and "api.github.com" in url:
        req.add_header("Authorization", f"Bearer {tok}")
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


rel = json.loads(get(f"https://api.github.com/repos/{REPO}/releases/latest"))
asset = next((a for a in rel["assets"] if ASSET.match(a["name"])), None)
if not asset:
    sys.exit(f"Keine ps5cleanup_v*.elf in {rel['tag_name']} gefunden")

digest = (asset.get("digest") or "").removeprefix("sha256:")
if not re.fullmatch(r"[0-9a-f]{64}", digest):
    digest = hashlib.sha256(get(asset["browser_download_url"], "application/octet-stream")).hexdigest()

catalog = {
    "name": "PS5 Cleanup",
    "payloads": [{
        "id": "ps5cleanup",
        "name": "PS5 Cleanup",
        "filename": asset["name"],
        "url": asset["browser_download_url"],
        "description": "Räumt eine PS5 mit Jailbreak auf wie frisch eingerichtet – ohne Neuinstallation. "
                       "Startet im Testmodus. / Cleans up a jailbroken PS5 like new, no reinstall. Starts in test mode.",
        "version": rel["tag_name"],
        "last_update": rel["published_at"][:10],
        "category": "Tools",
        "checksum": digest,
        "author": "da_Chris86",
        "details_url": f"https://raw.githubusercontent.com/{REPO}/main/README.md",
        "source": f"https://github.com/{REPO}/releases",
    }],
}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(catalog, f, ensure_ascii=False, indent=2)
    f.write("\n")
print(f"{rel['tag_name']}  {asset['name']}  {digest}")
