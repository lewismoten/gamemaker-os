# Notices

Copyright (c) 2026 Lewis Moten. Licensed under the [MIT license](LICENSE).

AI models used on this repository:

| Model | Tool | What it did |
| --- | --- | --- |
| Claude Opus 5.5 (Anthropic) | Claude Code | Wrote the README (2026-11-06) |
| Claude Opus 5.5 (Anthropic) | Claude Code | First program GMOS.ELF: games list, .GAM title reader, bitmap font (lowercase and punctuation drawn new), layout, input handling, host tests, build notes, board runs (2026-11-07) |
| Claude Opus 5.5 (Anthropic) | Claude Code | Playing games: the DOS file loader for the shared play core, the GIF port, the session (cards, ticks, keys), the clock and sleep in the main loop, host tests against the Pocket Player, the board driver and check, docs (2026-11-07) |

## Third-party code

- `sdk/`: the sandbox-kernel SDK (headers, `crt0.S`, `mem.c`, link map), MIT, copyright (c) 2026 Lewis Moten; the commit is in `sdk/PINNED`.
- `tools/font.txt`: the capitals, digits and `- : . /` come from sandbox-kernel's 5x7 status font (MIT, same author).
- `third_party/gmcore/` (`gmcore.h`, `gmmon.h`): the play core of the Pocket Player in [Game-Maker: Sandbox Edition](https://github.com/lewismoten/GameMaker-SandboxEdition), copied unchanged at the commit in `third_party/gmcore/PINNED`, with that repository's MIT license (`third_party/gmcore/LICENSE`: copyright (c) 2014 gandrewstone; copyright (c) 2026 Lewis Moten for the modifications). It ports that project's web engine, which follows `PLAYGAME.C` from RSD Game-Maker 3.0 (Recreational Software Designs, 1991-1995), whose source was released under the MIT License in 2014. The two files were written with Claude Sonnet 5.5 (Anthropic), as that repository's history and NOTICE.md record.
- `src/gif.c`: a C port of that repository's GIF decoder (`web/packages/engine/src/formats/gif.ts`, MIT, same license as above), which mirrors `GIFC.C` from the same RSD source.
- `src/game.cpp`: reads the `.GAM` layout as that repository's `formats/game.ts` documents it (from RSD's `GRATOR.H`) and builds levels the way its Pocket Player packer does; the code is new.
- `tests/games/DICE/`: the DOS files of the Starter game "Roll the Dice" from Game-Maker: Sandbox Edition (MIT, as above; the game's own `LICENSE.TXT` is included), used as a test fixture. `tests/data/dice-pocket.txt` and `dice-cards.txt` are output of that repository's Pocket Player host test and packed data (`tools/pocket_reference.py`).
