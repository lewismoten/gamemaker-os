#!/bin/zsh
# usage: profile_snd.sh <old jsrun> <new jsrun> <gmos root> <out dir> [game folder, default tests/games/DICE]
# Host-only checks of the Game-Maker profile (docs/js-profile.md) against the old JS copy, on a virtual clock:
#  1 sound   a game played 20 s: the SND log lines (what the audio side gets) match, and when the app stops the game
#            with stop(), the profile adds SND stopall/end (the old copy left the music running); Esc ends it the same
#  2 picture an <img src> on a game folder: same framebuffer (CRC) as the old copy
#  3 screen  the framebuffer mid-game (14 s) and after the stop: same CRC
#  4 leaks   the game started and stopped 50 times from one app, and the app started and stopped 50 times mid-game:
#            heap flat, "leaks 0 open 0", one SND end per stop
# Exits 1 on a failure. Prints the heap numbers for docs/js-profile.md.
OLD=$1; NEW=$2; R=$3; O=$4; GAME=${5:-$R/tests/games/DICE}; mkdir -p $O; fail=0
SD=$(mktemp -d); mkdir -p $SD/OS/APPS $SD/GAMES
for a in GPLAY GESC GLEAK GPIC; do ln -s $R/tests/js/apps/$a $SD/OS/APPS/$a; done
ln -s $GAME $SD/GAMES/G; ln -s $R/tests/games/DICE $SD/GAMES/H
ck() {cksum < $1 | cut -d' ' -f1,2;}
lines() {grep -E '^(SND|JS game|GMOS level)' $1;}
ok() {echo "PASS $1";}; bad() {echo "FAIL $1"; fail=1;}
for a in GPLAY:21000 GESC:15000; do n=${a%%:*}; ms=${a##*:}
  $OLD $SD /sd/OS/APPS/$n --vclock --ms $ms --shot $O/old$n.shot > $O/old$n.log 2>&1
  $NEW $SD /sd/OS/APPS/$n --vclock --ms $ms --shot $O/new$n.shot > $O/new$n.log 2>&1
  nl=$(grep -c '^SND' $O/new$n.log)
  if [ $n = GPLAY ]; then # stop(): the profile ends the sound; the old copy did not
    diff <(lines $O/oldGPLAY.log) <(lines $O/newGPLAY.log | grep -v -E '^SND (stopall|end)$') > /dev/null && grep -q '^SND end' $O/newGPLAY.log && ! grep -q '^SND end' $O/oldGPLAY.log && ok "sound GPLAY: $nl SND lines, same as the old copy plus stopall/end at stop()" || bad "sound GPLAY"
  else
    diff <(lines $O/old$n.log) <(lines $O/new$n.log) > /dev/null && ok "sound GESC: $nl SND lines identical (Esc)" || bad "sound GESC"
  fi
  [ "$(ck $O/old$n.shot)" = "$(ck $O/new$n.shot)" ] && ok "screen $n: framebuffer CRC $(ck $O/new$n.shot)" || bad "screen $n"
done
for w in old new; do exe=$OLD; [ $w = new ] && exe=$NEW; $exe $SD /sd/OS/APPS/GPLAY --vclock --ms 14000 --shot $O/${w}mid.shot > $O/${w}mid.log 2>&1; done
[ "$(ck $O/oldmid.shot)" = "$(ck $O/newmid.shot)" ] && [ "$(ck $O/newmid.shot)" != "$(ck $O/newGPLAY.shot)" ] && ok "screen mid-game (14 s, still playing): CRC $(ck $O/newmid.shot)" || bad "screen mid-game"
$OLD $SD /sd/OS/APPS/GPIC --vclock --ms 1500 --shot $O/oldpic.shot > $O/oldpic.log 2>&1
$NEW $SD /sd/OS/APPS/GPIC --vclock --ms 1500 --shot $O/newpic.shot > $O/newpic.log 2>&1
[ "$(ck $O/oldpic.shot)" = "$(ck $O/newpic.shot)" ] && ! grep -qi 'cannot show' $O/newpic.log && ok "picture: img src on game folders, CRC $(ck $O/newpic.shot)" || bad "picture"
$NEW $SD /sd/OS/APPS/GLEAK --vclock --ms 20000 --soak > $O/leak.log 2>&1
n=$(grep -c '^JS game' $O/leak.log); m1=$(grep 'APP n 1 ' $O/leak.log | grep -o 'mallocSize":[0-9]*' | cut -d: -f2); m50=$(grep 'APP n 50 ' $O/leak.log | grep -o 'mallocSize":[0-9]*' | cut -d: -f2)
grep -o 'HOST soak.*' $O/leak.log
[ "$n" = 50 ] && [ $((m50 - m1)) -lt 1024 ] && grep -q 'heap_blocks 0' $O/leak.log && ok "leaks: 50 play/stop, mallocSize $m1 -> $m50, heap_blocks 0" || bad "leaks play/stop (games $n, $m1 -> $m50)"
$NEW $SD /sd/OS/APPS/GPLAY --vclock --ms 3000 --cycles 50 > $O/cyc.log 2>&1
grep -q 'CYCLES 50 blocks first \([0-9]*\) last \1 leaks 0 open 0' $O/cyc.log && [ "$(grep -c '^SND end' $O/cyc.log)" = 50 ] && ok "leaks: 50 app starts stopped mid-game: $(grep CYCLES $O/cyc.log)" || bad "leaks app cycles"
rm -rf $SD; exit $fail
