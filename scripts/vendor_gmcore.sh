#!/bin/sh
# SPDX-License-Identifier: MIT
# Copy the Game-Maker play core (gmcore.h, gmmon.h and gmsmall.h, header-only, no board
# code) from the gamemaker-pocket checkout (the canonical copy, which builds the
# Pocket Player firmware) into third_party/gmcore/, unchanged (the LICENSE here
# is kept: it names this fork's changes), and record the commit in third_party/gmcore/PINNED.
# They are shared, never edited here: fix them in gamemaker-pocket first.
# Usage: scripts/vendor_gmcore.sh <gamemaker-pocket checkout> [commit]
set -eu
gm=$1; commit=${2:-main}
here=$(cd "$(dirname "$0")/.." && pwd)
rev=$(git -C "$gm" rev-parse "$commit")
out="$here/third_party/gmcore"
mkdir -p "$out"
for f in gmcore.h gmmon.h gmsmall.h; do git -C "$gm" show "$rev:src/$f" > "$out/$f"; done
printf 'gamemaker-pocket %s\nsrc/gmcore.h, src/gmmon.h and src/gmsmall.h of the gamemaker-pocket repository at that commit\n' "$rev" > "$out/PINNED"
echo "third_party/gmcore is gamemaker-pocket $rev"
