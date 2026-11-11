# Remote-only audit

Audited native `GMOS.ELF` (`menu.c`, `view.c`, `main.c`, `session.c`,
`hiscreen.c`, `saveplay.c`) and the separate `JS.ELF` app picker and sample
apps. Baseline input is Linux key events: Up 103, Down 108, Left 105,
Right 106, OK/Enter 28, Back/Esc 1. HDMI-CEC must translate into those events
in the kernel/input transport. No number keys, color keys, Tab, letters,
mouse, or keyboard modifiers are needed for these launcher flows.

The gap column records the initial audit and the resolution in this change.

| Screen / flow | What remote keys do | Gap / resolution |
| --- | --- | --- |
| Library category tabs | Up from the first row of the current page focuses tabs. Left/Right changes category; OK/Down enters it. Back returns to cards. Up from tabs opens Options. | Already navigable; Options now exposes commands beyond the tabs. Empty physical categories are omitted. |
| Library cards and pages | Arrows select cards and cross page boundaries. OK opens details. Back opens Options. | Back previously stayed still; Q was required to quit. Options fixes this. |
| Search entry | Enter Search through the tabs with OK/Down. Arrows choose letters/digits; OK types. Space, Delete, Clear, Done, `-`, `_` are selectable actions. Results filter title/company as each character is entered. | Letter grid already existed; retained and tested. No physical typing is required. |
| Search results / no matches | Done switches arrows to results; OK opens details. Up from the first row returns to tabs; OK on Search reopens its letter grid. Back clears a nonempty query first, then returns to tabs. | Already reachable, including an empty result set. Back repeats do not clear/leave. Search is case-insensitive and holds 24 characters; use a word/substring for titles containing other punctuation. |
| Favorites | Reach through tabs even when empty. OK opens details. Choose Favorite in details or Options to add/remove the selected game. | F had an existing details-button alternative; Options adds access from cards. Removing the last favorite keeps details attached to that game until Back. |
| Recent | Reach through tabs even when empty. Arrows select; OK opens details. A launched game moves to the front when play begins. | Already remote accessible. The board owns recording/persisting launches; host tests simulate that callback. |
| Open by name | Back on cards → Options → Open by folder name. Arrows/OK enter the DOS folder alias; Delete edits, Clear resets, Done opens details; Back cancels. OK on Play then launches. | Previously `/`, typed letters and Enter only. Added the shared letter grid with digits, spaces, hyphen and underscore (eight-character alias limit). A miss reports “not found” and preserves selection. This is the folder alias; Search finds display titles/company names. |
| Game details | Arrows choose Play, Back, Favorite, High scores, Music. OK presses; Back returns to originating cards/results. | F/H/M already had button alternatives. Existing keyboard shortcuts remain available. Idle demo previews stop on any press; folder entry suppresses previews. |
| Options / music setting | Back on cards, or Up from tabs. Arrows choose Back to library, Favorite, High scores, Music, Open by folder name, Quit. OK activates; Back cancels. | New visible alternatives to F/H/M/Q and `/`. Music starts off and is a session setting. Empty-library Options still allows Music and Quit; game-specific commands do nothing without a selection. |
| Empty/unreadable library diagnostics | Back opens Options; Back cancels it. Options → Quit exits. | Previously only Q could exit; diagnostics now advertise Back. |
| DOS menu before play | Up/Down chooses Play, Instructions, Storyline, Credits, High scores, Quit; OK activates. Back returns to library details. | Already accessible. DOS Quit also returns to library details. |
| Title, instructions, story, credits, level pages | OK advances timed/key-wait pages (after their transition). Back returns to DOS menu before play; during a running game it finishes the session through scores/menu. | Already accessible. Missing optional pages show a message then return; Back also returns. |
| In-game pause | Back pauses; Up/Down chooses Resume, Save, Load, Quit game. OK activates. Back resumes. Quit game goes through Top Ten, then the DOS menu, where Back returns to library. | Existing remote alternatives to F5/F6; retained and tested. |
| Save / Load | Up/Down chooses one of ten slots; OK loads or opens save-name entry. The shared letter grid names a save; Done saves. Back cancels; OK/Back dismisses messages. | Save-name grid already existed. Error and persistence behavior is unchanged. |
| Top Ten name entry | Letter grid appears immediately for a qualifying score. Arrows/OK type up to 22 characters; Space/Delete/Clear edit; Done accepts and shows table. Back accepts the current name, including empty, and shows table. | Grid already existed. Back preserves the original DOS acceptance semantics. Shared grid now also has hyphen/underscore. |
| Top Ten table | OK or Back returns to its caller (details, library or DOS menu); any press dismisses, or it expires after 30 seconds. | Already accessible; key releases/repeats do not dismiss. |
| Quit GMOS | Options → Quit exits to the kernel through the normal cleanup path. | New remote alternative to Q, including with no games installed. |
| JS app picker (separate JS.ELF) | Up/Down selects apps, OK launches. Back quits to kernel. | Previously F10 only for quitting the picker; added Back, including an empty app list. |
| JS Settings | Arrows focus Theme, Hints, Reset; OK activates. Back returns to app picker. | Controls already navigable; exit was Q/F10 only. Added Esc. Settings remain memory-only by design. |
| JS Clock | Arrows focus buttons; OK changes theme. Back returns to app picker. | Added Esc as a remote alternative to Q/F10. |
| JS Files | Arrows select/page; OK opens folder/text. Back leaves text, then ascends folders; at card root Back returns to app picker. | Root Back previously stayed at root, requiring Q/F10 to exit. |

## Scope and verification

Native GMOS and JS apps are separate executables; GMOS does not launch Settings
or other JS apps. Benchmark/demo apps are developer tools, not library flows.
Game-specific movement/action bindings still come from the original game: the
six-key launcher contract does not remap arbitrary DOS gameplay controls.
CEC hardware translation and TV-specific key delivery need device verification;
the host suite drives the production consumers of translated key events.

`tests/test_remote_only.py` uses only arrows, Enter and Esc for tab traversal,
Search, Favorites, Recent, options, settings/music, open-by-name editing,
miss/cancel, alias punctuation, empty-library quit, details/Play, DOS pages,
scores and pause/return. `tests/test_collections.py` additionally types DICE
through the Search grid and exercises paging. `tests/host/hiscreen.c` enters,
edits and commits DICE using only remote keys. `tests/host/saveplay.c` scenario
7 clears/names/saves a slot with remote keys; `test_dosmenu.py` covers pause
Save/Load requests. `test_app_exit.py` drives Settings controls and Back through
Settings, Clock, Files and the picker in one host runtime. Host rendering writes
image artifacts without opening windows; audio tests use buffers/fake devices.

Run `make all test`. No sanitizer, DYLD injection, crash probes, interactive
windows or sound playback are required.
