---
name: transition-development
description: Add or change bmatrix image motion and character-space effects.
---

Read AGENTS.md. Keep image sampling transforms before glyph conversion and character effects afterward. Render from explicit time, seed, dimensions, and immutable source data. No wall-clock reads in the pure renderer. Require exact endpoints, deterministic frames, stable glyph choices, and defined resize behavior. Test progress zero, intermediate progress, and completion. Compare perceived motion in a timed preview; frame snapshots alone cannot establish smoothness.
