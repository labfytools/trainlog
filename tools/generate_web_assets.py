#!/usr/bin/env python3
"""Generate a deterministic, bounded C manifest from a Vite dist tree."""

from __future__ import annotations

import hashlib
import pathlib
import sys

MAX_ASSETS = 128
MAX_ASSET_BYTES = 4 * 1024 * 1024
MAX_TOTAL_BYTES = 16 * 1024 * 1024
MIME = {
    ".css": "text/css; charset=utf-8",
    ".html": "text/html; charset=utf-8",
    ".ico": "image/x-icon",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".png": "image/png",
    ".svg": "image/svg+xml",
    ".webp": "image/webp",
}


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def byte_array(name: str, data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in chunk) + ",")
    return f"static const unsigned char {name}[] = {{\n" + "\n".join(lines) + "\n};\n"


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_web_assets.py DIST OUTPUT_C")
    root = pathlib.Path(sys.argv[1]).resolve()
    output = pathlib.Path(sys.argv[2])
    files = sorted(path for path in root.rglob("*") if path.is_file())
    if not files or len(files) > MAX_ASSETS:
        raise SystemExit(f"nombre d'assets invalide: {len(files)}")
    assets: list[tuple[str, str, bytes, str, bool]] = []
    total = 0
    for path in files:
        relative = path.relative_to(root)
        if any(part in ("", ".", "..") for part in relative.parts):
            raise SystemExit(f"chemin d'asset invalide: {relative}")
        mime = MIME.get(path.suffix.lower())
        if mime is None:
            raise SystemExit(f"type d'asset non autorisé: {relative}")
        data = path.read_bytes()
        if len(data) > MAX_ASSET_BYTES:
            raise SystemExit(f"asset trop grand: {relative}")
        total += len(data)
        if total > MAX_TOTAL_BYTES:
            raise SystemExit("taille totale des assets dépassée")
        url = "/" + relative.as_posix()
        etag = '"sha256-' + hashlib.sha256(data).hexdigest() + '"'
        immutable = relative.parts[0] == "assets"
        assets.append((url, mime, data, etag, immutable))
    if not any(url == "/index.html" for url, *_ in assets):
        raise SystemExit("index.html absent du manifeste")

    parts = [
        "/* Generated deterministically by tools/generate_web_assets.py. */\n",
        '#include "web_assets.h"\n\n',
        "#include <string.h>\n\n",
    ]
    for index, (_, _, data, _, _) in enumerate(assets):
        parts.append(byte_array(f"trainlog_web_data_{index}", data))
        parts.append("\n")
    parts.append("static const TrainlogWebAsset trainlog_web_assets[] = {\n")
    for index, (url, mime, data, etag, immutable) in enumerate(assets):
        parts.append(
            f"    {{{c_string(url)}, {c_string(mime)}, trainlog_web_data_{index}, "
            f"{len(data)}U, {c_string(etag)}, {'true' if immutable else 'false'}}},\n"
        )
    parts.append("};\n\n")
    parts.append(
        "bool trainlog_web_assets_available(void) { return true; }\n\n"
        "const TrainlogWebAsset *trainlog_web_asset_find(const char *path)\n"
        "{\n"
        "    size_t index;\n"
        "    if (path == NULL) return NULL;\n"
        "    for (index = 0U; index < sizeof(trainlog_web_assets) /\n"
        "            sizeof(trainlog_web_assets[0]); ++index)\n"
        "        if (strcmp(path, trainlog_web_assets[index].path) == 0)\n"
        "            return &trainlog_web_assets[index];\n"
        "    return NULL;\n"
        "}\n\n"
        "const TrainlogWebAsset *trainlog_web_index_asset(void)\n"
        "{\n"
        "    return trainlog_web_asset_find(\"/index.html\");\n"
        "}\n"
    )
    output.write_text("".join(parts), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
