# bmatrix

Repository and homepage: [github.com/matthewbohl/bmatrix](https://github.com/matthewbohl/bmatrix).

Photographs concealed inside a perpetual Matrix-style ASCII field. The glyph
streams continue to cascade and shimmer while density, brightness, and (in full
mode) a trace of source hue let the image emerge when you relax your gaze. C++20,
CMake, libpng, and terminfo. Classic green is the default; monochrome and full-color
ASCII use the same image geometry and glyph mapping.

The default Matrix field uses the same normal-terminal character range as CMatrix
(`!` through `z`) and its every-other-column stream spacing. That spacing is part
of the classic low-resolution look; bmatrix uses the missing columns as visual air,
not additional image pixels.

The image has a deliberately dim, fixed ghost layer on the stream columns. Brighter
characters fall across it continuously, so a viewer can recover the subject without
the screensaver becoming a static ASCII photograph. `--image-strength` controls that
layer and its influence on stream density; the default is `0.7`.

This is the first development milestone, not a public release. It is an original
implementation inspired by [cmatrix](https://github.com/abishekvashok/cmatrix);
no cmatrix source or fonts are included.

## Build

On macOS: `brew install cmake libpng`. The platform supplies curses/terminfo.
On Debian/Ubuntu: `sudo apt install build-essential cmake libpng-dev libncurses-dev python3`.

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
./build-release/bmatrix path/to/photo.png
```

No arguments displays a generated demo image. This milestone accepts PNG and
8-bit binary PPM (P6). JPEG, animated input files, and additional effects are later
milestones. A still image supplies the hidden signal; the Matrix field never settles
until quit or replay.

```sh
bmatrix photo.png                         # classic green
bmatrix photo.png --mode mono             # white/grayscale characters
bmatrix photo.png --mode full             # source-image colors
bmatrix photo.png --color 256             # explicit capability ceiling
bmatrix photo.png --color none            # no color sequences
bmatrix photo.png --effect matrix          # perpetual field; default
bmatrix photo.png --image-strength 1       # strongest persistent image layer
bmatrix photo.png --effect none            # static image
bmatrix photo.png --effect flow --duration 4 # legacy reveal experiment
bmatrix photo.png --size 160x54 --output plain --time 2.5
bmatrix photo.png --size 160x54 --output json --time 2.5
```

Keys: `q` quits, Space pauses, `r` replays. Ctrl-C exits cleanly. Ctrl-Z restores
the terminal before suspension; `fg` resumes. Resizing refits the image while
preserving the current field phase.

Artistic `--mode` and terminal `--color` are independent. Auto uses terminfo's
palette plus `COLORTERM=truecolor`/`24bit` for RGB; `NO_COLOR` disables automatic
color. Explicit RGB is available when detection is conservative. ASCII is used
in all modes; there are no special fonts, Unicode-width assumptions, graphics
protocols, or runtime network requests. Unsupported cursor-addressed terminals
can use a deterministic plain Matrix snapshot at time zero. Use `--time` to
inspect another point in the field without terminal control codes.

Default cell width/height is 0.5. Adjust `--cell-aspect` for your terminal font
if a circle appears stretched. `--size` is limited to 500x200 cells; PNG dimensions
are bounded to 16384 per side and 32 megapixels. Alpha is composited on black.

## Development and quality

Read [AGENTS.md](AGENTS.md) for proposal/counterproposal, assumption, and visual
quality rules. Narrow skills live in `.agents/skills`. See
[architecture](docs/architecture.md) and [release gates](docs/releasing.md).

The attached development photograph is stored only in ignored
`.local/test-photo.png`. CI uses generated fixtures. Source archives explicitly
exclude local photos and artifacts.

```sh
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
python3 scripts/source-archive.py /tmp/bmatrix-0.1.0.tar.gz
```

`scripts/visual-preview.py` optionally uses Pillow to render logical frames into
PNG comparisons and a looping Matrix GIF; Pillow is a developer-only dependency.
These previews help assess concealed-image clarity and continuous motion, but actual terminal appearance
depends on its font and palette. PTY tests verify control sequences and lifecycle;
they do not certify every emulator. Visual and platform checks must pass before
shipping, as documented in `docs/releasing.md`.

MIT licensed. libpng, zlib, and ncurses retain their respective licenses and are
provided as system dependencies.
