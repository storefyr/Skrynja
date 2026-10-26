"""M0.0b: vibe-acp --version + licens via pakke-metadata."""
import json
import os
import subprocess

ENV = dict(os.environ)
ENV["PYTHONPATH"] = "/workspace/malelab"

p = subprocess.run(["/usr/local/bin/vibe-acp", "--version"], capture_output=True, text=True, env=ENV, timeout=30)
print("vibe-acp --version =>", repr(p.stdout.strip()), "rc:", p.returncode)

p2 = subprocess.run(["/usr/local/bin/vibe", "--version"], capture_output=True, text=True, env=ENV, timeout=30)
print("vibe --version     =>", repr(p2.stdout.strip()), "rc:", p2.returncode)

p3 = subprocess.run(["/usr/local/bin/pip", "show", "mistral-vibe", "agent-client-protocol", "mcp"], capture_output=True, text=True, timeout=30)
for block in p3.stdout.split("\n\n"):
    keep = [l for l in block.splitlines() if l.startswith(("Name:", "Version:", "License:", "License-Expression:", "Home-page:"))]
    if keep:
        print(" | ".join(keep))
