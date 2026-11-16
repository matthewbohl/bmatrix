#!/usr/bin/env python3
"""Fail closed unless release evidence covers the exact source archive."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tarfile


def archived_metadata(archive):
    """Read bounded regular metadata files without extracting an untrusted tar."""
    result = {}
    roots = set()
    with tarfile.open(archive, "r:gz") as source:
        for member in source:
            parts = PurePosixPath(member.name).parts
            if len(parts) != 3 or parts[1] != "debian" or parts[2] not in ("control", "changelog"):
                continue
            name = parts[2]
            if name in result or not member.isfile() or member.size > 128 * 1024:
                raise ValueError(f"invalid or duplicate archived debian/{name}")
            roots.add(parts[0])
            with source.extractfile(member) as content:
                result[name] = content.read().decode("utf-8")
    if set(result) != {"control", "changelog"} or len(roots) != 1:
        raise ValueError("archive must contain debian/control and debian/changelog under one source directory")
    return result


def control_field(control, name):
    # Release identity comes from the source stanza, not a binary-package stanza.
    source_stanza = re.split(r"\n\s*\n", control, maxsplit=1)[0]
    values = re.findall(rf"^{name}:\s*(.+)$", source_stanza, re.MULTILINE)
    if len(values) != 1:
        raise ValueError(f"archived debian/control requires one {name} field")
    return values[0].strip()


def address(value):
    match = re.fullmatch(r"[^<>]+<([^<>]+)>", value)
    if not match:
        raise ValueError("archived Debian maintainer must include an email address")
    return match[1].strip()

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("archive", type=Path)
evidence_arg = parser.add_mutually_exclusive_group(required=True)
evidence_arg.add_argument("--evidence", type=Path)
evidence_arg.add_argument("--evidence-env", help="Environment variable containing evidence JSON")
args = parser.parse_args()
try:
    evidence = json.loads(args.evidence.read_text() if args.evidence else os.environ[args.evidence_env])
    archive_hash = hashlib.sha256(args.archive.read_bytes()).hexdigest()
    errors = []
    if evidence.get("source_sha256") != archive_hash:
        errors.append("evidence does not match the exact source archive SHA-256")
    homepage = evidence.get("homepage", "")
    if not re.fullmatch(r"https://[^/\s]+(?:/[^\s]*)?", homepage) or any(
        marker in homepage.lower() for marker in ("example.", ".invalid", "localhost", "placeholder")
    ):
        errors.append("a real public HTTPS homepage is required")
    maintainer = evidence.get("maintainer_email", "")
    if not re.fullmatch(r"[^\s@]+@[^\s@]+\.[^\s@]+", maintainer) or any(
        marker in maintainer.lower() for marker in ("example.", ".invalid", "placeholder")
    ):
        errors.append("a real maintainer email is required")
    metadata = archived_metadata(args.archive)
    if control_field(metadata["control"], "Homepage") != homepage:
        errors.append("archived debian/control Homepage does not match release evidence")
    if address(control_field(metadata["control"], "Maintainer")) != maintainer:
        errors.append("archived debian/control Maintainer does not match release evidence")
    trailer = re.search(r"^ -- (.+?)  \S.*$", metadata["changelog"], re.MULTILINE)
    if not trailer or address(trailer[1]) != maintainer:
        errors.append("archived debian/changelog maintainer does not match release evidence")
    checks = evidence.get("checks", {})
    required = ("linux_gcc", "linux_clang", "macos_arm64", "macos_x86_64",
                "asan_ubsan", "source_archive_install", "debian_offline_build",
                "lintian", "autopkgtest", "piuparts", "homebrew_style",
                "homebrew_audit", "homebrew_install_test")
    for name in required:
        check = checks.get(name, {})
        if check.get("status") != "passed" or not str(check.get("evidence", "")).strip():
            errors.append(f"missing successful check and evidence: {name}")
    reviews = evidence.get("visual_reviews", [])
    for mode in ("green", "mono", "full"):
        for size in ("80x24", "160x50"):
            matching = [review for review in reviews if review.get("mode") == mode and review.get("size") == size]
            if not any(review.get("approved") is True and review.get("animation_reviewed") is True
                       and str(review.get("reviewer", "")).strip()
                       and re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(review.get("date", "")))
                       and str(review.get("evidence", "")).strip() for review in matching):
                errors.append(f"missing human still-image and animation approval: {mode} {size}")
    if errors:
        raise ValueError("\n".join(errors))
except (OSError, KeyError, ValueError, TypeError, AttributeError, UnicodeError, tarfile.TarError) as error:
    print(f"RELEASE BLOCKED: {error}", file=sys.stderr)
    sys.exit(1)
print(f"Release evidence accepted for {archive_hash}; publishing still requires authorization.")
