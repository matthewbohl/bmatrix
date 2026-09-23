---
name: terminal-rendering
description: Change bmatrix terminal capabilities, output, input, resizing, or lifecycle handling.
---

Read AGENTS.md. Keep one screen-output owner. Use terminfo for capability-dependent controls; gate direct RGB sequences on capability or explicit override. Artistic palette and color capability are separate. Test unsupported TERM, non-TTY output, tiny sizes, resize, interrupt, and suspension with a PTY. Verify cursor and termios restoration. Restrict glyphs to printable single-column ASCII. Do not infer actual emulator compatibility from TERM-only tests.
