#!/usr/bin/env python3
"""Kontrol af spec-manifest.json mod faktiske filer.

Fejler med exit 1, hvis blob-SHA, bytes, linjeantal eller et sektionsinterval
afviger, eller hvis en normativ fil i manifestet mangler i træet.
Kør fra repo-roden. Kun stdlib. kræver git.

Bemærk: main er IKKE branch-beskyttet (målt 2026-10-29 via GitHub API,
branches/main -> protected: false), så denne kontrol er en test på PR'er,
ikke en port mod direkte push til main.
"""
import json
import subprocess
import sys
from pathlib import Path

def repo_root() -> Path:
    """Repo-roden: gå op fra denne fil til første forælder med .git."""
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / ".git").exists():
            return p
        p = p.parent
    # fallback: to niveauer op (dokumenter/vibe-acp -> repo)
    return Path(__file__).resolve().parent.parent.parent


ROOT = repo_root()
MANIFEST = ROOT / "dokumenter" / "vibe-acp" / "spec-manifest.json"


def git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True
    ).stdout


def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    head = git("rev-parse", "HEAD").strip()
    fejl: list[str] = []

    for row in manifest["filer"]:
        sti = row["sti"]
        # findes filen i HEAD?
        try:
            blob = git("rev-parse", f"HEAD:{sti}").strip()
        except subprocess.CalledProcessError:
            fejl.append(f"{sti}: mangler i HEAD (normativ fil i manifest uden række i træet)")
            continue
        if blob != row["blob_sha"]:
            fejl.append(f"{sti}: blob-SHA afviger: manifest={row['blob_sha']} HEAD={blob}")
        data = subprocess.run(
            ["git", "-C", str(ROOT), "cat-file", "-p", f"HEAD:{sti}"],
            capture_output=True, check=True,
        ).stdout
        if len(data) != row["bytes"]:
            fejl.append(f"{sti}: bytes afviger: manifest={row['bytes']} målte={len(data)}")
        linjer = data.decode("utf-8").count("\n") + (0 if data.endswith(b"\n") or not data else 1)
        # nøjagtig linjetælling: antal \n (fil antages newline-afsluttet)
        linjer = data.count(b"\n")
        if linjer != row["linjer"]:
            fejl.append(f"{sti}: linjeantal afviger: manifest={row['linjer']} målte={linjer}")
        # sektionsintervaller: hver start-linje skal indeholde overskriften
        tekst = data.decode("utf-8").splitlines()
        for sektion in row.get("sektioner", []):
            start = sektion["start_linje"]
            if not (1 <= start <= len(tekst)):
                fejl.append(f"{sti}: sektion {sektion['overskrift']!r} start {start} uden for filen ({len(tekst)} linjer)")
            elif sektion["overskrift"] not in tekst[start - 1]:
                fejl.append(
                    f"{sti}: sektion {sektion['overskrift']!r} forventet på linje {start}, "
                    f"men linjen er: {tekst[start - 1]!r}"
                )

    if fejl:
        print("MANIFEST-KONTROL FEJLEDE:", file=sys.stderr)
        for f in fejl:
            print("  -", f, file=sys.stderr)
        print(f"(kontrolleret mod HEAD {head})", file=sys.stderr)
        return 1
    print(f"manifest OK: {len(manifest['filer'])} filer, HEAD {head}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
