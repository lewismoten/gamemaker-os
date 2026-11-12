# Finding and keeping games

The Games page uses the Pocket Player's labels and behavior: case-insensitive
substring search, a yellow star for Favorites, and Recently played in newest-first
order. GMOS also searches the company in its library index. Search reads the
metadata already in memory; typing never opens game assets.

- **Up** from the top card row focuses the tabs. **Left/Right** chooses a tab;
  **Enter/Down** enters it. **Tab**, **Shift+Tab**, **[** and **]** also change tabs.
- Choose **Search**, type up to 24 characters, and watch the title/company matches
  and count change. **Backspace** edits, **arrows** select a result, **Enter** opens
  Details. **Esc** clears the query; another Esc leaves the field for the tabs.
  The query survives a visit to Details and a tab change. Entering Search from
  the focused tab with Enter/Down also opens an on-screen keyboard: arrows choose
  a letter or Space/Delete/Clear/Done, Enter presses it. **Done** returns to the
  results. Up from the keyboard's first row returns to the tabs. Physical typing
  hides this keyboard automatically. Tab onto Search for direct keyboard entry.
- **F** on a game card or Details toggles its favorite star. The Details **Favorite**
  button does the same using arrows and Enter. Search consumes letters as text;
  open Details to favorite a search result. Favorites holds up to 32 games, with
  the most recently starred first. A full list asks you to unstar one first.
- **Recent** is the tab for **Recently played**, with the newest game first and
  at most 20 games. A repeated launch moves the game to the front without
  duplicating it. It updates when gameplay starts (including a loaded game),
  once per library launch. Details, idle demos and the game's title/menu do not
  change it.
- Details has **Play**, **Back**, **Favorite**, **High scores**, and **Music: On/Off**
  buttons. Arrows choose, **OK/Enter** presses; **Back/Esc** returns to the cards.
  **H**, **M**, **F**, Space, Tab and Shift+Tab remain keyboard shortcuts.
- Move Down past a page's last row to reach the next page; Left across its first
  card returns to the previous page. Up from the first row of any page focuses
  tabs. **PgUp/PgDn**, Home and End still work.
- **Back/Esc** on cards opens **Options**, including with an empty library.
  Up from tabs also opens it. Choose **Quit** to exit; **Q** remains a shortcut.
  Options also exposes Favorite, High scores, Music, Open by folder name, UI sounds and
  saved [Screen off after / Power saving](power.md) controls.

The existing development command is unchanged: **/**, exact folder alias, **Enter**
opens Details across categories, ignoring case. **Shift+Enter** also presses Play.
It accepts DOS letters/digits, hyphen and underscore, with Backspace to edit and
Esc to cancel. Use the Search tab for partial title/company matching.

Favorites and Recent remain visible when empty, with counts for installed games.
Category counts retain their original meaning; Search counts matches across all
categories. Missing games stay in the saved lists but are omitted from the page.

## Remote audit: every screen

Native launcher actions use Linux arrows (103/105/106/108), OK/Enter (28),
Back/Esc (1). The kernel must translate remote keys to these events; no digit,
channel or color button is required. See the full [remote-only audit](remote-only.md),
including the separate JS picker and sample apps. CEC delivery still needs a Pi check.

| Screen | Arrows + OK + Back path | Keyboard extras |
| --- | --- | --- |
| Games/category tabs | Up from the current page's top row, Left/Right tab, OK/Down enter; arrows cross page boundaries; Back on cards or Up from tabs opens Options | Tab/Shift+Tab, brackets, Home/End, PgUp/PgDn; Q quits |
| Search | Enter tab to show keyboard; arrows choose letters/digits/Space/Delete/Clear/Done, OK types and filters live; Done browses results, OK opens Details; Back clears then returns to tabs | Physical typing hides keyboard, Backspace edits; slash opens exact folder alias |
| Favorites/Recent | Same tabs, cards and paging, including empty tabs; Details retains game identity after unstarring | F favorites, Recent updates on actual play/load |
| Options | Arrows select Back/Favorite/High scores/Music/Open by folder name/Quit/UI sounds/Screen off after/Power saving; OK activates, Back cancels | Shortcuts on cards: F/H/M/Q and slash |
| Open by folder name | Options opens the shared keyboard; Done opens details for an exact DOS alias (up to eight characters); Back cancels | Slash, type alias, Enter; Shift+Enter also plays |
| Details | Arrows select Play/Back/Favorite/High scores/Music, OK presses; Back returns to cards | F/H/M, Space, Tab/Shift+Tab |
| Top Ten list | OK or Back returns; existing 30-second timeout remains | Any key dismisses |
| Top Ten name | Shared on-screen keyboard, Done accepts; Back accepts current name as before | Type directly, Backspace/Left erase, Enter accepts after typing; Up/Down restores keyboard |
| DOS title/intro/pages | OK advances a page; Back returns to the DOS menu | Any key advances |
| DOS menu/instructions/story/credits | Up/Down chooses, OK opens; Back returns from pages to menu, then to library | Key-based page advance remains |
| In-game pause | Back pauses; Up/Down selects Resume/Save game/Load game/Leave game, OK activates; Back resumes; Leave runs the score check and returns to DOS menu | Existing gameplay key mappings remain; F5/F6 open save/load directly |
| Save/Load slots | Up/Down selects one of ten slots, OK opens; Back cancels | Enter/keypad Enter selects |
| Save name | Shared keyboard edits existing slot name, Clear replaces it, Done saves; Back cancels without saving | Physical typing and Backspace/Left; Enter accepts after typing; Up/Down restores keyboard |
| Save/Load result | OK/Back dismisses, or wait two seconds | Any key dismisses |

Search, folder aliases, Top Ten and save names use the same bounded text-entry component
(`src/osk.h`), with letters/digits, hyphen, underscore and Space/Delete/Clear/Done. Search filters
after each edit; Top Ten retains its 22-character limit and save names their
20-character limit. OK repeats never insert text or activate Done. Direct keyboard
input retains case and punctuation. DOS text pages display content without asking for text.
Open by folder name uses the DOS alias, not the display title or long path; a
miss preserves selection. Search finds partial title/company matches.

Pause freezes game ticks and releases held controls. Resuming resets the play
clock, so time spent in menus is never replayed as game ticks. Save/load requests
use the same persistent save layer and screens as F5/F6.

## Input and details limits

Input reads hold at most 16 events; nested screens drain at most four batches
per pass. EOF, closed devices and malformed byte counts trigger recovery;
EAGAIN means no input. Removal/reopen and SYN_DROPPED clear launcher Shift/repeat
state; gameplay releases held controls. Reattachment snapshots clear queued
input rather than replaying held keys. Launcher repeats need a seen press,
a timestamp no older than 100 ms and at least 100 ms since the last accepted
press/repeat. Fresh presses/releases remain ordered. These are host-tested
policies, not a claim of USB hub or CEC hardware qualification.

Details shows the page before artwork reads, then presents/heartbeats at
50 ms cooperative checkpoints during reads, decode and archive inflation.
Reads are chunked to 16 KiB; a blocking syscall cannot be interrupted, and
loading does not dispatch input. Keys queue until loading finishes.
Title assets use a 1 MiB buffer; slide lists read at most 4095 bytes and 32
entries, with bounded nesting. GIF/BKD output is a 320x200 window; FLI/FLC
canvases are limited to 64,000 pixels. PCX is unsupported. Bad or missing
artwork falls through to alternatives, then “No title screen”; this is not
complete validation of every malformed game file.

Frame cost: the board has no hardware divide, so a details frame must not divide per
pixel (its gradient did, 2.4 million divisions, over a second: the 2 s heartbeat fault on
First Steps). The panel now uses one reciprocal; `tests/test_frame_cost.py` fails past the
division and pixel-store budgets. The demo preview runs at most four ticks and 40 ms per
step, draws after every step and drops missed ticks (slow motion, not a frozen picture).
While the picture is read, its placeholder says "Loading..." (not "No title screen").

Index title/company/blurb fields hold at most 63/31/639 bytes. Details titles
wrap to three lines; blurbs use the remaining space above y=440, with `...`
when text remains. There is no description scrolling. `test_details_bounds.py`
checks decoder canaries, truncated/mutated files and cooperative presents;
`test_input.py` covers repeat bursts, hotplug during loading and malformed reads.

## Card storage

Kernel ABI 1.7 or newer stores `/sd/OS/SAVES/GMOS/FAVS.TXT` and
`/sd/OS/SAVES/GMOS/RECENT.TXT` (on the card: `/OS/SAVES/GMOS/`).
Both are small ASCII files beginning with `GMOS 1` and a newline, followed by one
game key per line in newest-first order, ending with a newline. For example:

```text
GMOS 1
AB12CD34
12345678
```

Game keys are eight uppercase hexadecimal digits: the full 32-bit FNV-1a hash of
`scan-path + "/" + GAM-name`. Full paths distinguish folders and packed/ZIP games
that share a DOS alias. Changing the title/index keeps the key; moving or renaming
a game changes it. Keys are local to this card layout, not Pocket Player UIDs.

Missing files mean empty lists. Bad headers and oversized files are rejected;
invalid, duplicate and incomplete trailing records are ignored. Unknown game keys
are retained. Reads are bounded to 320 bytes. Writes use the existing save layer:
`FAVS.TMP` or `RECENT.TMP`, sync, close, rename over the real file. A leftover TMP
is never loaded. Changes happen in memory even if storage is unavailable; a failed
favorite save shows **Changes not saved**, and a failed Recent save is logged.
Older kernels keep the lists for the running session only.
`SETTINGS.DAT` beside them stores native power settings through the same save
layer; see [defaults, errors and ABI behavior](power.md).

## Host verification

`make test` exercises remote-only search for DICE, favorite toggling, Details scores/music,
paging, pause/resume/save/load requests, Top Ten name entry, save-name entry and
keyboard regressions, along with search counts,
newest-first ordering and caps, text round trips, short reads/writes, missing and
malformed data, and an interrupted replacement. The existing open-by-folder tests
still drive the same agent helper key sequence. `make all js quota` builds both
board programs and checks image quotas.

Host-rendered 960x540 examples (fixture library, including Roll the Dice title art):

- [Search](library-search.png)
- [Remote search keyboard](library-keyboard.png)
- [Details: Favorite, High scores and Music buttons](library-favorite-details.png)
- [Top Ten remote name keyboard](top-ten-keyboard.png)
- [Favorites](library-favorites.png)
- [Recently played](library-recent.png)

The host screenshots use the production drawing code; `make test` refreshes copies
in `build/host/library-*.png`. Hardware/CEC behavior
still needs a board test.
