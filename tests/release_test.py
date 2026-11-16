#!/usr/bin/env python3
"""Exercise release metadata checks with synthetic archives/evidence, not approvals."""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest


CHECKER = Path(__file__).resolve().parents[1] / "scripts/release-check.py"
HOMEPAGE = "https://github.com/matthewbohl/bmatrix"
EMAIL = "fixture@bmatrix.test"


class ReleaseTests(unittest.TestCase):
    def run_check(self, *, control_email=EMAIL, changelog_email=EMAIL, homepage=HOMEPAGE,
                  evidence_email=EMAIL, evidence_homepage=HOMEPAGE, omit=None, duplicate=False):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "fixture.tar.gz"
            contents = {
                "control": f"Source: bmatrix\nMaintainer: Fixture <{control_email}>\nHomepage: {homepage}\n\nPackage: bmatrix\n",
                "changelog": f"bmatrix (0.1.0-1) unstable; urgency=medium\n\n  * Synthetic fixture.\n\n -- Fixture <{changelog_email}>  Mon, 16 Nov 2026 12:00:00 +0000\n",
            }
            with tarfile.open(archive, "w:gz") as target:
                for name, content in contents.items():
                    if name == omit:
                        continue
                    encoded = content.encode()
                    member = tarfile.TarInfo(f"bmatrix-0.1.0/debian/{name}")
                    member.size = len(encoded)
                    target.addfile(member, io.BytesIO(encoded))
                    if duplicate and name == "control":
                        target.addfile(member, io.BytesIO(encoded))
            # These test-only records exercise the parser; they are neither
            # persisted nor presented as evidence of real build/visual approval.
            evidence = {
                "source_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                "homepage": evidence_homepage, "maintainer_email": evidence_email,
                "checks": {name: {"status": "passed", "evidence": "synthetic fixture"}
                           for name in ("linux_gcc", "linux_clang", "macos_arm64", "macos_x86_64",
                                        "asan_ubsan", "source_archive_install", "debian_offline_build",
                                        "lintian", "autopkgtest", "piuparts", "homebrew_style",
                                        "homebrew_audit", "homebrew_install_test")},
                "visual_reviews": [{"mode": mode, "size": size, "approved": True,
                                    "animation_reviewed": True, "reviewer": "synthetic fixture",
                                    "date": "2026-11-16", "evidence": "synthetic fixture"}
                                   for mode in ("green", "mono", "full")
                                   for size in ("80x24", "160x50")],
            }
            return subprocess.run([sys.executable, str(CHECKER), str(archive),
                                   "--evidence-env", "BMATRIX_TEST_EVIDENCE"],
                                  env=dict(os.environ, BMATRIX_TEST_EVIDENCE=json.dumps(evidence)),
                                  capture_output=True, text=True)

    def test_matching_archived_metadata(self):
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_placeholder_cannot_be_repaired_by_evidence(self):
        result = self.run_check(control_email="maintainers@bmatrix.invalid",
                                changelog_email="maintainers@bmatrix.invalid")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Maintainer does not match", result.stderr)

    def test_placeholder_in_both_evidence_and_archive_rejected(self):
        result = self.run_check(control_email="maintainers@bmatrix.invalid",
                                changelog_email="maintainers@bmatrix.invalid",
                                evidence_email="maintainers@bmatrix.invalid")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("real maintainer email", result.stderr)

    def test_homepage_mismatch(self):
        result = self.run_check(homepage="https://github.com/wrong/project")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Homepage does not match", result.stderr)

    def test_changelog_mismatch(self):
        result = self.run_check(changelog_email="other@bmatrix.test")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("changelog maintainer does not match", result.stderr)

    def test_missing_metadata(self):
        result = self.run_check(omit="control")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must contain debian/control", result.stderr)

    def test_duplicate_metadata(self):
        result = self.run_check(duplicate=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate archived debian/control", result.stderr)


if __name__ == "__main__":
    unittest.main()
