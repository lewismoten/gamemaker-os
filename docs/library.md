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
- **Esc** in Details returns to the cards. **Esc/Q** in the regular grid quits.
  **H** shows high scores, **M** toggles library music, **PgUp/PgDn** pages.

The existing development command is unchanged: **/**, exact folder alias, **Enter**
opens Details across categories, ignoring case. **Shift+Enter** also presses Play.
It accepts DOS letters/digits, hyphen and underscore, with Backspace to edit and
Esc to cancel. Use the Search tab for partial title/company matching.

Favorites and Recent remain visible when empty, with counts for installed games.
Category counts retain their original meaning; Search counts matches across all
categories. Missing games stay in the saved lists but are omitted from the page.

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

## Host verification

`make test` exercises search, counts, remote navigation, favorite toggling,
newest-first ordering and caps, text round trips, short reads/writes, missing and
malformed data, and an interrupted replacement. The existing open-by-folder tests
still drive the same agent helper key sequence. `make all js quota` builds both
board programs and checks image quotas.

Host-rendered 960x540 examples (fixture library, including Roll the Dice title art):

- [Search](library-search.png)
- [Remote search keyboard](library-keyboard.png)
- [Favorite in Details](library-favorite-details.png)
- [Favorites](library-favorites.png)
- [Recently played](library-recent.png)

The host screenshots use the production drawing code; `make test` refreshes copies
in `build/host/library-*.png`. Hardware/CEC behavior
still needs a board test.
