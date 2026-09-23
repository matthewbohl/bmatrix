#!/usr/bin/env python3
"""Create a deterministic, explicit-allowlist archive without local photos."""
import argparse
import gzip
import io
from pathlib import Path
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
members = ["CMakeLists.txt", "CMakePresets.json", "LICENSE", "README.md",
           "AGENTS.md", "include", "src", "tests", "man", "debian", "docs",
           "scripts", "packaging", ".agents", ".github"]
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open("wb") as out, gzip.GzipFile(fileobj=out, mode="wb", filename="", mtime=0) as gz:
    with tarfile.open(fileobj=gz, mode="w") as archive:
        for member in members:
            path = root / member
            if not path.exists():
                raise SystemExit(f"Required source member missing: {member}")
            paths = sorted(path.rglob("*")) if path.is_dir() else [path]
            for item in paths:
                if not item.is_file() or "__pycache__" in item.parts:
                    continue
                if item.is_symlink():
                    raise SystemExit(f"Source symlinks are not allowed: {item}")
                data = item.read_bytes()
                info = tarfile.TarInfo("bmatrix-0.1.0/" + item.relative_to(root).as_posix())
                info.size = len(data)
                info.mode = 0o755 if item.stat().st_mode & 0o111 else 0o644
                info.mtime = 0
                archive.addfile(info, io.BytesIO(data))
print(args.output.resolve())
