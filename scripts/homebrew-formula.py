#!/usr/bin/env python3
"""Render a local-only formula for the source archive supplied as argument."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", type=Path)
parser.add_argument("--output", type=Path, help="Write generated formula to this file")
args = parser.parse_args()
archive = args.archive.resolve()
template = Path(__file__).resolve().parent.parent / "packaging/homebrew/bmatrix.rb.in"
formula = template.read_text().replace("@SOURCE_URL@", archive.as_uri()).replace(
    "@SOURCE_SHA256@", hashlib.sha256(archive.read_bytes()).hexdigest())
if args.output:
    args.output.write_text(formula)
else:
    print(formula, end="")
