# UI sounds

Short, quiet blips for GMOS's own menus (library cards, tabs, details, Options and the on-screen keyboards).
Never inside a running game: a game's pause and quit menus are the game's own and stay silent.

| Cue | When | Sound |
| --- | --- | --- |
| Move | cursor between cards, buttons, Options rows, keyboard keys | a dry tick, 52 ms |
| Tab | category tab changes | two notes up, 106 ms |
| Select | OK/Enter, opening details or Options, a toggle turned on | a rising four-note chime, 240 ms |
| Back | Back/Esc leaves details, Options, search or an entry | two notes down, 150 ms |
| Can't | arrow at the edge of the list, Favorites full, Favorite or High scores with no game, a folder that is not found | two low notes, 130 ms |

Typing letters and the M key make no UI sound (M only toggles music).

**Setting.** Options > **UI sounds: On/Off**, saved in `SETTINGS.DAT` with Screen off after and Power saving
(`GMOS 1`, `S P U`; an older 11-byte file loads with sounds on). Default on.

**How it is mixed.** `gm_event` names the cue (`menu.cue`, `src/uisound.h`; always none when the setting is off);
`main.c` calls `au_ui`, which sets one small voice in the audio mix (`src/uisound.c`). Two-operator FM notes (src/uisound.c)
in the OPL2 manner, added after the master volume, so a silent library (music off) still blips and the song, the PC
speaker and VOC clips are never touched. One blip at a time, up to 240 ms (Select): a move or tab that arrives while a blip
is still sounding is dropped (a burst of held arrows makes a ticking, not a queue); Select, Back and Can't replace a
move. Nothing waits and nothing allocates. Host test: `tests/host/uisound_check.c`.

**Latency.** The blip is rendered into the same ring as the song, so it is heard after what the ring already holds
(up to about 300 ms while music plays). With the library silent the ring is kept shallow (`au_quiet`, audio.h), so the blip is heard sooner; a shallow, silent ring running dry is not counted as an underrun.
