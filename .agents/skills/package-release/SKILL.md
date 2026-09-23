---
name: package-release
description: Change bmatrix Debian or Homebrew packaging, CI release gates, or source distributions.
---

Read AGENTS.md and docs/releasing.md. Use CMake install rules and build from a clean source archive with no Git metadata. Package only licensed source and generated fixtures. Never include .local personal photos. Verify declared dependencies and offline Debian builds. Run Debian lintian, autopkgtest, and piuparts; run Homebrew style, audit, install, and test. Keep platform and visual gates explicit and blocking. Publishing requires user authorization; preparing and locally testing artifacts is normal development work. Do not claim acceptance into official archives.
