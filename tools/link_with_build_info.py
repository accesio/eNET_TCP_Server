#!/usr/bin/env python3
"""Generate build metadata, link atomically, and commit a build number on success."""

from __future__ import annotations

import argparse
import datetime as dt
import fcntl
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from typing import Dict, Iterable


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--counter", required=True)
    parser.add_argument("--version-file", required=True)
    parser.add_argument("--generated-cpp", required=True)
    parser.add_argument("--generated-object", required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--compile-flags", default="")
    parser.add_argument("--link-flags", default="")
    parser.add_argument("--libs", default="")
    parser.add_argument("--increment", action="store_true")
    args, objects = parser.parse_known_args()
    if objects and objects[0] == "--":
        objects = objects[1:]
    if not objects:
        parser.error("at least one object file is required after --")
    return args, objects


def parse_version_file(path: Path) -> Dict[str, int]:
    values: Dict[str, int] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if ":=" in line:
            key, value = line.split(":=", 1)
        elif "=" in line:
            key, value = line.split("=", 1)
        else:
            continue
        values[key.strip()] = int(value.strip(), 10)
    required = ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_PATCH")
    missing = [key for key in required if key not in values]
    if missing:
        raise RuntimeError(f"missing version fields: {', '.join(missing)}")
    return values


def git_value(arguments: Iterable[str], fallback: str = "unknown") -> str:
    override_name = {
        ("rev-parse", "--short=12", "HEAD"): "GIT_COMMIT",
        ("describe", "--always", "--dirty", "--tags"): "GIT_DESCRIBE",
    }.get(tuple(arguments))
    if override_name and os.environ.get(override_name):
        return os.environ[override_name]
    try:
        return subprocess.check_output(
            ["git", *arguments], text=True, stderr=subprocess.DEVNULL
        ).strip() or fallback
    except (OSError, subprocess.CalledProcessError):
        return fallback


def build_time_utc() -> dt.datetime:
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch:
        return dt.datetime.fromtimestamp(int(epoch), tz=dt.timezone.utc)
    return dt.datetime.now(tz=dt.timezone.utc)


def cxx_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'

def command_words(value: str, option_name: str) -> list[str]:
    """Split a shell-style command supplied by make into subprocess argv words."""
    words = shlex.split(value)
    if not words:
        raise RuntimeError(f"{option_name} did not specify a compiler command")
    return words

def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        if path.read_text(encoding="utf-8") == content:
            return
    except FileNotFoundError:
        pass
    tmp = path.with_name(path.name + f".tmp.{os.getpid()}")
    tmp.write_text(content, encoding="utf-8")
    os.replace(tmp, path)


def atomic_write_counter(path: Path, value: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=path.name + ".", dir=str(path.parent))
    try:
        os.fchmod(fd, 0o644)
        with os.fdopen(fd, "w", encoding="ascii") as stream:
            stream.write(f"{value}\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def main() -> int:
    args, objects = parse_args()
    output = Path(args.output)
    counter = Path(args.counter)
    lock_path = counter.with_suffix(counter.suffix + ".lock")
    lock_path.parent.mkdir(parents=True, exist_ok=True)

    with lock_path.open("a+", encoding="ascii") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        try:
            current = int(counter.read_text(encoding="ascii").strip() or "0")
        except FileNotFoundError:
            current = 0
        candidate = current + 1 if args.increment else current

        version_fields = parse_version_file(Path(args.version_file))
        major = version_fields["VERSION_MAJOR"]
        minor = version_fields["VERSION_MINOR"]
        patch = version_fields["VERSION_PATCH"]
        version = f"{major}.{minor}.{patch}.{candidate}"
        timestamp = build_time_utc()
        build_utc = timestamp.strftime("%Y-%m-%dT%H:%M:%SZ")
        build_date = timestamp.strftime("%Y-%m-%d")
        build_clock = timestamp.strftime("%H:%M:%SZ")
        git_hash = git_value(("rev-parse", "--short=12", "HEAD"))
        git_describe = git_value(("describe", "--always", "--dirty", "--tags"))

        source = f'''// Generated by tools/link_with_build_info.py.  Do not edit.\n#include "build_info.h"\n\nnamespace BuildInfo\n{{\nconst unsigned Major = {major}U;\nconst unsigned Minor = {minor}U;\nconst unsigned Patch = {patch}U;\nconst unsigned Build = {candidate}U;\nconst char Version[] = {cxx_string(version)};\nconst char ProductVersion[] = {cxx_string("aioenetd " + version)};\nconst char GitHash[] = {cxx_string(git_hash)};\nconst char GitDescribe[] = {cxx_string(git_describe)};\nconst char BuildUtc[] = {cxx_string(build_utc)};\nconst char BuildDate[] = {cxx_string(build_date)};\nconst char BuildTime[] = {cxx_string(build_clock)};\n}} // namespace BuildInfo\n'''
        generated_cpp = Path(args.generated_cpp)
        generated_object = Path(args.generated_object)
        write_if_changed(generated_cpp, source)
        generated_object.parent.mkdir(parents=True, exist_ok=True)

        cxx_cmd = command_words(args.cxx, "--cxx")
        compile_cmd = [*cxx_cmd, *shlex.split(args.compile_flags), "-I.",
                       "-c", str(generated_cpp), "-o", str(generated_object)]
        subprocess.run(compile_cmd, check=True)

        output.parent.mkdir(parents=True, exist_ok=True)
        temporary_output = output.with_name(output.name + f".new.{os.getpid()}")
        link_cmd = [*cxx_cmd, *shlex.split(args.link_flags), "-o", str(temporary_output),
                    *objects, str(generated_object), *shlex.split(args.libs)]
        try:
            subprocess.run(link_cmd, check=True)
            if args.increment:
                atomic_write_counter(counter, candidate)
            os.replace(temporary_output, output)
        finally:
            try:
                temporary_output.unlink()
            except FileNotFoundError:
                pass

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
    except Exception as error:
        print(f"build-info link failed: {error}", file=sys.stderr)
        raise SystemExit(1)
