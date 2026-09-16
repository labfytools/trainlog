#!/usr/bin/env python3
"""Contract tests for the bounded deterministic Web asset generator."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def run(generator: pathlib.Path, root: pathlib.Path, output: pathlib.Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(generator), str(root), str(output)],
        check=False,
        text=True,
        capture_output=True,
    )


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_generate_web_assets.py GENERATOR")
    generator = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="trainlog-web-assets-") as temporary:
        base = pathlib.Path(temporary)
        dist = base / "dist"
        (dist / "assets").mkdir(parents=True)
        (dist / "index.html").write_text("<script src='/assets/z.js'></script>", encoding="utf-8")
        (dist / "assets" / "z.js").write_text("export const z=1;", encoding="utf-8")
        (dist / "assets" / "a.css").write_text(":root{color:#cdd6f4}", encoding="utf-8")
        first = base / "first.c"
        second = base / "second.c"
        if run(generator, dist, first).returncode != 0 or run(generator, dist, second).returncode != 0:
            return 1
        first_bytes = first.read_bytes()
        if first_bytes != second.read_bytes():
            return 1
        text = first_bytes.decode("utf-8")
        if text.index('"/assets/a.css"') > text.index('"/assets/z.js"'):
            return 1

        excessive = base / "excessive"
        excessive.mkdir()
        (excessive / "index.html").write_text("ok", encoding="utf-8")
        for index in range(128):
            (excessive / f"asset-{index:03d}.js").write_text("0", encoding="ascii")
        if run(generator, excessive, base / "excessive.c").returncode == 0:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
