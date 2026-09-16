#!/usr/bin/env python3
"""Build the locked Trainlog frontend into a Meson-owned directory."""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: build_web_frontend.py NPM WEB_ROOT DIST STAMP")
    npm = pathlib.Path(sys.argv[1])
    web_root = pathlib.Path(sys.argv[2]).resolve()
    dist = pathlib.Path(sys.argv[3]).resolve()
    stamp = pathlib.Path(sys.argv[4]).resolve()
    if not (web_root / "package-lock.json").is_file():
        raise SystemExit("package-lock.json absent")
    if not (web_root / "node_modules").is_dir():
        raise SystemExit("web/node_modules absent; exécutez 'npm ci' dans web/")
    if dist.exists():
        shutil.rmtree(dist)
    subprocess.run(
        [str(npm), "run", "build", "--", "--outDir", str(dist), "--emptyOutDir"],
        cwd=web_root,
        check=True,
    )
    if not (dist / "index.html").is_file():
        raise SystemExit("Vite n'a pas produit index.html")
    stamp.write_text("trainlog-web-build-v1\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
