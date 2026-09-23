# bmatrix working agreement

## Lead agent

- For every task, state a proposal and a credible counterproposal, choose one with a brief reason, and proceed. Use this for meaningful design decisions, not every edit.
- Make reasonable assumptions instead of asking the user to choose routine implementation details. Record consequential assumptions in the handoff or architecture notes. Ask only when required authority or essential information cannot be inferred; do not bypass tool permission controls.
- C++20 and CMake are the accepted implementation stack. The executable and package name is `bmatrix`.
- Default to classic Matrix green. Support monochrome and full-color ASCII as separate artistic modes; terminal color capability is an independent setting.
- Delegate bounded implementation, packaging, and independent review work when useful. The lead integrates and verifies the whole result. Start with three workstreams: media/rendering, terminal/CLI, packaging/quality.
- Provide honest verification results. Configuring a CI check is not the same as running it. Never claim a release or platform is verified when it is not.

## Product quality

Anything shipped must be smooth and beautiful, using cmatrix's fluidity and restraint as the reference. Visual quality is a release requirement, not optional polish.

- Preserve image aspect ratio, recognizable subjects, deliberate spacing, stable glyphs, and readable contrast. No whole-screen flashing or gratuitous random glyph churn.
- Animate against a monotonic clock with deadline pacing. Skip expired deadlines instead of emitting a backlog. No busy loops; pause must remain responsive.
- One component owns screen output. Never mix curses refresh with independent ANSI painting.
- Keep image-space animation separate from character-space effects. Preserve exact transition endpoints.
- Use printable, single-column ASCII initially. No graphics protocols, special fonts, or truecolor requirement.
- Restore terminal modes and cursor on normal exit, handled interruption, and exceptions; test suspension/resumption and resize. Do not claim cleanup after SIGKILL.
- Bound image decoding, terminal dimensions, and memory use. Keep diagnostics outside active animation output.

## Verification and release

- Test deterministic frames, image fitting, palette fallback, CLI failures, PTY lifecycle, and pacing. Verify an actual visual preview with the user's local image.
- Run builds and relevant tests before handing off changes. CI must cover Linux and macOS, warnings, sanitizers, and packaging.
- Release gates include source-archive builds, Debian package lint/install tests, Homebrew style/audit/test, and recorded visual approval of green, monochrome, and full color at representative sizes. Unverified gates block release.
- Keep personal test photos local and ignored; never include them in packages or release archives. Use generated, redistributable fixtures for CI.
- Packaging must use the CMake install rules. No network fetches during Debian builds. APT repository/tap delivery and official archive admission are different milestones.
- Consult the narrow skills under `.agents/skills` when changing rendering, transitions, visual tests, or release packaging.
