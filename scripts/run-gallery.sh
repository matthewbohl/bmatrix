#!/bin/sh
# Generate visual fixtures, then run bmatrix; press n/N for the next image.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gallery_dir=${BMATRIX_GALLERY_DIR:-"$root/artifacts/test-gallery"}
binary=${1:-"$root/build/bmatrix"}
if [ "$#" -gt 0 ]; then shift; fi
python3 "$root/scripts/generate-test-images.py" "$gallery_dir"
exec "$binary" --gallery "$gallery_dir" "$@"
