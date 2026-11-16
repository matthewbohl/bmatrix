#!/usr/bin/env python3
"""Check that the visual gallery is reproducible and renderable in every palette."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
GENERATOR = Path(__file__).resolve().parents[1] / "scripts/generate-test-images.py"


class FixtureTests(unittest.TestCase):
    def test_gallery_reproducible_and_renderable(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = (Path(directory) / name for name in ("first", "second"))
            for output in (first, second):
                subprocess.run([sys.executable, str(GENERATOR), str(output)], check=True,
                               capture_output=True)
            images = sorted(first.glob("*.ppm"))
            self.assertEqual(len(images), 15)
            self.assertEqual([path.name for path in images], sorted(path.name for path in second.glob("*.ppm")))
            for image in images:
                self.assertEqual(image.read_bytes(), (second / image.name).read_bytes())
                for mode in ("green", "mono", "full"):
                    with self.subTest(image=image.name, mode=mode):
                        result = subprocess.run([BINARY, str(image), "--mode", mode, "--size", "80x24",
                                                 "--output", "json", "--time", "10"], check=True,
                                                capture_output=True, text=True)
                        frame = json.loads(result.stdout)
                        self.assertEqual(len(frame["cells"]), 80 * 24)
                        self.assertTrue(any(glyph != 32 and max(red, green, blue) > 0
                                            for glyph, red, green, blue in frame["cells"]))


if __name__ == "__main__":
    unittest.main()
