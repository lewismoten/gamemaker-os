#!/bin/zsh
# usage: cmp.sh <old jsrun> <new jsrun> <gmos root> <out dir>
OLD=$1; NEW=$2; R=$3; O=$4; mkdir -p $O
SD=$(mktemp -d); mkdir -p $SD/OS/APPS $SD/GAMES
ln -s $R/apps/HELLO $SD/OS/APPS/HELLO; ln -s $R/tests/games/DICE $SD/GAMES/DICE
# one app dir with an APP.JSON for the new core, if needed
case1="9000|500:28:1,520:28:0,1000:28:1,1020:28:0,3000:28:1,3050:28:0,5000:28:1,5050:28:0"
case2="9000|500:28:1,520:28:0,1000:28:1,1020:28:0,6000:1:1,6020:1:0"
case3="25000|500:28:1,520:28:0,1000:28:1,1020:28:0,3000:28:1,3050:28:0,5000:28:1,5050:28:0,8000:106:1,9000:106:0,12000:57:1,12500:57:0"
i=0; for c in $case1 $case2 $case3; do i=$((i+1)); ms=${c%%|*}; k=${c##*|}
  for w in old new; do exe=$OLD; [ $w = new ] && exe=$NEW
    $exe $SD /sd/OS/APPS/HELLO --vclock --ms $ms --keys $k --shot $O/$w$i.shot > $O/$w$i.log 2>&1; echo "$w$i rc=$?" 
  done
  echo "case$i shot crc: $(cksum < $O/old$i.shot) | $(cksum < $O/new$i.shot)"
done
