# First milestone decisions

Proposal: C++20/CMake with a deterministic image-to-cell core and one terminfo
screen owner. Counterproposal: Rust/Cargo for stronger memory safety. C++ remains
the accepted baseline for conventional terminal and package integration.

The pipeline is decode -> aspect-correct area sampling -> latent image signal ->
perpetual character-space Matrix field -> terminal frame diff. The image is not
painted as a fixed terminal photograph: it modulates stream density, brightness,
and full-mode hue. A deterministic horizontal displacement of row bands is applied
in image space before glyph conversion.

`core.hpp` defines images, cells, frames, artistic palettes, and pure rendering.
`image.cpp` decodes PNG through the system libpng simplified API and P6 PPM for
small fixtures. `core.cpp` fits and samples the source, chooses stable density
glyphs, and applies a deterministic perpetual Matrix field. The finite flow reveal
remains for transition experiments and reproducible snapshots. `terminal.cpp` owns termios,
signals, capabilities, diff output, and monotonic pacing. `main.cpp` parses and
validates the CLI, and exposes deterministic plain/JSON frame export.
Gallery mode retains sorted paths and decodes only the selected image, releasing
the previous pixels before advancing. Paused display invalidations repaint the
same animation time, including resize and suspension/resumption.

Proposal: system libpng initially. Counterproposal: vendored stb_image gives more
formats immediately. PNG is sufficient for the supplied photograph and makes
decoder updates the package manager's responsibility. JPEG and animated sources
will require explicit decoder and frame-timing decisions.

Cells contain printable ASCII and foreground RGB on black. Spaces are black
coverage in this initial single-layer renderer. Transparency between scene layers
is intentionally not represented yet; PNG alpha is composited to black at decode.
Do not infer future compositing semantics from a blank glyph.

The original image stays immutable. Matrix mode currently resamples on each frame
to animate its image-space displacement; caching a reduced sampling surface is a
future performance improvement. The latent image signal changes slowly while stream
glyphs and heads move continuously with their column. The default field uses
CMatrix's normal terminal character range (ASCII 33–122) and its two-column
stream spacing. A dim, stable ghost layer preserves image legibility through both
brightness and stable, dithered glyph coverage, while brighter stream cells animate
above it. Its 0–1 strength is explicit in `RenderOptions` and
defaults to 0.7, so zero has no latent-photo contribution and one is deliberately
more legible. At Matrix startup, column heads enter from above and expose the
persistent layer only as they pass; the first trail continues exiting naturally
after its head reaches the bottom. Ambient trails fade in with independent phases.
The source is also resampled through a small, independently drifting
three-row-band horizontal offset before glyph conversion; row energy is normalized
before persistent coverage is dithered. Timing is explicit inside the pure effect; only the
terminal event loop reads the clock. Terminal output contains changed cells only,
buffered per frame. Curses screen-refresh functions are never used alongside the
custom renderer.

Assumptions: initial platforms are POSIX Linux and macOS. Initial fixtures are PNG
or binary PPM. Terminal cells default to a width/height ratio of 0.5, configurable
because fonts differ. The source photo is private development data. MIT applies
to the new original implementation; no upstream cmatrix code was copied.
