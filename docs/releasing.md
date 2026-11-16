# Release gates

This milestone is a development build, not a release. Packaging metadata uses an
explicit `.invalid` maintainer address until a release maintainer address is
established. The canonical repository and homepage is
https://github.com/matthewbohl/bmatrix. No release workflow publishes artifacts.

Build with `cmake --preset release`, `cmake --build --preset release`, and
`ctest --preset release`. CMake alone owns installation of the binary and man page.
System dependencies are libpng and ncurses/terminfo; builds do not download code.
Python is required for development tests, not for the installed executable.

Run `python3 scripts/source-archive.py /tmp/bmatrix-0.1.0.tar.gz` to create an
allowlisted archive. Extract it outside the checkout, build, test, and stage an
installation. Archives exclude `.local`, Git metadata, and build directories.
Personal photographs must remain inside ignored `.local/`; only generated
fixtures belong in tests. The archive's allowlist does not replace license review.

For Debian, install the declared Build-Depends, build using `dpkg-buildpackage
-us -uc -b`, run `lintian --fail-on error` on the changes file, then run
`autopkgtest` and `piuparts` in a clean Debian stable environment. Confirm builds
work with network access disabled after dependencies are installed. CI configures
these checks; their presence is not proof that they have passed. Before a public
release, replace the maintainer placeholder and verify current policy warnings.

For Homebrew, generate a development formula with
`python3 scripts/homebrew-formula.py /tmp/bmatrix-0.1.0.tar.gz`, save its output as
`bmatrix.rb` inside a local tap, then run `brew style`, `brew install --build-from-source`,
`brew test`, and `brew audit --strict` against that tap. The generated formula
uses a local file URL and the canonical GitHub homepage. It is sufficient for
install/test work; a public release archive is still required for distribution.
Before publishing, use a real immutable HTTPS release archive and its SHA-256;
repeat audit and tests with those exact values. Do not point at a release tag
until it exists. Ordinary CI runs full formula style/build/test; the optional
`release_readiness` workflow input additionally runs the strict metadata audit.
Skipping that audit is not a passing release gate.

A release remains blocked until all of the following have recorded evidence for
the same source revision:

- Linux GCC and Clang, macOS Clang, warnings-as-errors, unit and PTY tests.
- Address/undefined-behavior sanitizers, including error and cleanup paths.
- Clean source archive build and staged install, with no private image assets.
- Debian lintian, autopkgtest, piuparts, and offline package build.
- Homebrew style, strict audit, source install, and formula test on macOS;
  both Intel and Apple Silicon must be verified before claiming both.
- Human visual approval of green, mono, and full at 80x24 and 160x50: recognizable
  subjects, correct proportions, deliberate contrast, no distracting flicker,
  smooth paced reveal, pause, resize, interruption, and suspend/resume.

Record visual evidence and reviewer/date in a release review associated with the
exact revision. A static preview is not animation approval. Unrun or failed checks
block release; do not label CI configuration as validation. Source publishing,
signed APT repository/tap publishing, and official Debian/Homebrew admission are
separate activities. Publishing requires user authorization.

## Executable release gate

Run `python3 scripts/release-check.py /tmp/bmatrix-0.1.0.tar.gz --evidence
/absolute/path/release-evidence.json`. Missing, malformed, incomplete, or mismatched
evidence fails closed. Store evidence outside the source tree/archive to avoid a
self-referential archive hash. Evidence is an accountable review record, not a
substitute for executing the checks or independently verifying attached logs.

The JSON must contain `source_sha256`, an actual HTTPS `homepage`, and an actual
`maintainer_email`. The checker reads the source archive itself and requires its
Debian source-stanza `Homepage` and `Maintainer`, plus the latest changelog trailer's
maintainer, to match that evidence. Updating the JSON alone cannot finalize the
development package's placeholder metadata. `checks` maps each required name below to an object with
`status: "passed"` and a nonempty `evidence` log/artifact reference:

`linux_gcc`, `linux_clang`, `macos_arm64`, `macos_x86_64`, `asan_ubsan`,
`source_archive_install`, `debian_offline_build`, `lintian`, `autopkgtest`,
`piuparts`, `homebrew_style`, `homebrew_audit`, `homebrew_install_test`.

`visual_reviews` must contain all six mode/size combinations above, each with
`mode`, `size`, `approved: true`, `animation_reviewed: true`, `reviewer`, ISO `date`,
and an `evidence` reference. Do not invent approvals. The workflow's optional
release-readiness input accepts this JSON as `release_evidence`; its final gate
depends on the build/package jobs and validates the exact archive generated in CI.
