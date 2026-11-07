# Notices

Copyright (c) 2026 Lewis Moten. Licensed under the [MIT license](LICENSE).

AI models used on this repository:

| Model | Tool | What it did |
| --- | --- | --- |
| Claude Opus 5.5 (Anthropic) | Claude Code | Wrote the README (2026-11-06) |
| Claude Opus 5.5 (Anthropic) | Claude Code | First program GMOS.ELF: games list, .GAM title reader, bitmap font (lowercase and punctuation drawn new), layout, input handling, host tests, build notes, board runs (2026-11-07) |

## Third-party code

- `sdk/`: the sandbox-kernel SDK (headers, `crt0.S`, `mem.c`, link map), MIT, copyright (c) 2026 Lewis Moten; the commit is in `sdk/PINNED`.
- `tools/font.txt`: the capitals, digits and `- : . /` come from sandbox-kernel's 5x7 status font (MIT, same author).
