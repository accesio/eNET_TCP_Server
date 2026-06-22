#!/usr/bin/env python3
"""Recursively pack WebControl/www into webctl_assets.inc.

Run from the project root or from inside WebControl:

    python WebControl/Tools/pack_web_assets.py

The packer includes every ordinary file below WebControl/www.  Text files may
use simple include directives; included content is expanded in the packed output
and in the optional dist tree.

Supported include spellings:

    // #include "js/file.js"
    /* #include "css/file.css" */
    <!-- #include "partials/file.html" -->
    #include "text-or-fragment.txt"

Include paths are relative to the including file unless they begin with '/', in
which case they are relative to WebControl/www.  Cycles are reported as errors.
"""

from __future__ import annotations

import argparse
import hashlib
import mimetypes
import os
import re
import shutil
import sys
from pathlib import Path

INCLUDE_RE = re.compile(
    r"^\s*(?://\s*)?(?:/\*\s*)?(?:<!--\s*)?#include\s+[\"<]([^\">]+)[\">](?:\s*\*/)?(?:\s*-->)?\s*$"
)
TEXT_EXTS = {
    ".html", ".htm", ".css", ".js", ".json", ".svg", ".txt", ".xml", ".map", ".md"
}


def find_webcontrol_root() -> Path:
    here = Path(__file__).resolve()
    for candidate in [here.parent.parent, *here.parents]:
        if (candidate / "www").is_dir() and (candidate / "webctl.h").is_file():
            return candidate
    raise SystemExit("cannot find WebControl root containing www/ and webctl.h")


def is_text_file(path: Path) -> bool:
    return path.suffix.lower() in TEXT_EXTS


def include_target(www: Path, including: Path, include: str) -> Path:
    if include.startswith("/"):
        target = www / include.lstrip("/")
    else:
        target = including.parent / include
    try:
        target.resolve().relative_to(www.resolve())
    except ValueError:
        raise SystemExit(f"include escapes www tree: {including.relative_to(www)} -> {include}")
    return target


def expand_text(path: Path, www: Path, stack: tuple[Path, ...] = ()) -> str:
    path = path.resolve()
    if path in stack:
        cycle = " -> ".join(str(p.relative_to(www)) for p in (*stack, path))
        raise SystemExit(f"include cycle: {cycle}")
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        raise SystemExit(f"{path.relative_to(www)} is not valid UTF-8: {exc}") from exc

    out: list[str] = []
    for line in text.splitlines(keepends=True):
        match = INCLUDE_RE.match(line.rstrip("\r\n"))
        if not match:
            out.append(line)
            continue
        target = include_target(www, path, match.group(1))
        if not target.is_file():
            raise SystemExit(f"missing include {match.group(1)!r} referenced by {path.relative_to(www)}")
        if not is_text_file(target):
            raise SystemExit(f"binary include is not supported: {target.relative_to(www)}")
        out.append(expand_text(target, www, (*stack, path)))
        if out and not out[-1].endswith("\n"):
            out.append("\n")
    return "".join(out)


def load_asset(path: Path, www: Path) -> bytes:
    if is_text_file(path):
        return expand_text(path, www).encode("utf-8")
    return path.read_bytes()


def c_identifier(rel: Path) -> str:
    s = "webctl_asset_" + "_".join(rel.parts)
    return re.sub(r"[^0-9A-Za-z_]", "_", s)


def c_bytes(data: bytes) -> str:
    if not data:
        return "    0x00"
    lines = []
    for i in range(0, len(data), 12):
        chunk = data[i:i + 12]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk))
    return ",\n".join(lines)


def mime_for(path: Path) -> str:
    suffix = path.suffix.lower()
    if suffix in {".html", ".htm"}:
        return "text/html; charset=utf-8"
    if suffix == ".css":
        return "text/css; charset=utf-8"
    if suffix == ".js":
        return "application/javascript; charset=utf-8"
    if suffix == ".svg":
        return "image/svg+xml"
    if suffix == ".json":
        return "application/json"
    guessed, _ = mimetypes.guess_type(path.name)
    return guessed or "application/octet-stream"


def write_dist(www: Path, dist: Path, assets: list[tuple[Path, bytes]]) -> None:
    if dist.exists():
        shutil.rmtree(dist)
    dist.mkdir(parents=True, exist_ok=True)
    for rel, data in assets:
        target = dist / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--web-root", type=Path, default=None, help="source www directory; default WebControl/www")
    parser.add_argument("--output", type=Path, default=None, help="generated include path; default WebControl/webctl_assets.inc")
    parser.add_argument("--dist", type=Path, default=None, help="optional expanded filesystem www output")
    args = parser.parse_args()

    if sys.version_info < (3, 8):
        print("pack_web_assets.py requires Python 3.8 or newer", file=sys.stderr)
        return 2

    root = find_webcontrol_root()
    www = (args.web_root or (root / "www")).resolve()
    output = (args.output or (root / "webctl_assets.inc")).resolve()

    if not www.is_dir():
        print(f"web root not found: {www}", file=sys.stderr)
        return 2

    rel_files = sorted(
        p.relative_to(www)
        for p in www.rglob("*")
        if p.is_file()
    )
    if not rel_files:
        print(f"no files found under {www}", file=sys.stderr)
        return 2

    assets: list[tuple[Path, bytes]] = []
    for rel in rel_files:
        assets.append((rel, load_asset(www / rel, www)))

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("/*\n")
        f.write(" * Generated WebControl embedded assets.\n")
        f.write(" * Source: WebControl/www/\n")
        f.write(" * Regenerate with: python WebControl/Tools/pack_web_assets.py\n")
        f.write(" */\n\n")

        for rel, data in assets:
            ident = c_identifier(rel)
            f.write(f"static const unsigned char {ident}[] = {{\n")
            f.write(c_bytes(data))
            f.write("\n};\n\n")

        f.write("static const WebCtlAsset webctl_assets[] = {\n")
        for rel, data in assets:
            ident = c_identifier(rel)
            path = "/" + rel.as_posix()
            etag = hashlib.sha256(data).hexdigest()[:16]
            f.write(f"    {{ \"{path}\", \"{mime_for(rel)}\", {ident}, {len(data)}u, \"{etag}\" }},\n")
        f.write("};\n\n")
        f.write("static const unsigned webctl_asset_count = (unsigned)(sizeof(webctl_assets) / sizeof(webctl_assets[0]));\n")

    if args.dist is not None:
        write_dist(www, args.dist.resolve(), assets)

    print(f"packed {len(assets)} assets -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
