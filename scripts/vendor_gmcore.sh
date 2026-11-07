#!/bin/sh
# SPDX-License-Identifier: MIT
# Copy the Game-Maker play core (gmcore.h and gmmon.h) and the .GMP bundle reader
# (gmsmall.h), header-only, no board
# code, from a Game-Maker: Sandbox Edition checkout into third_party/gmcore/,
# unchanged, with that repository's LICENSE, and record the commit in
# third_party/gmcore/PINNED. The same files build the Pocket Player firmware
# there; they are shared, never edited here.
# Usage: scripts/vendor_gmcore.sh <GameMaker-SandboxEdition checkout> [commit]
set -eu
gm=$1; commit=${2:-origin/develop}
here=$(cd "$(dirname "$0")/.." && pwd)
rev=$(git -C "$gm" rev-parse "$commit")
out="$here/third_party/gmcore"
rm -rf "$out"; mkdir -p "$out"
for f in gmcore.h gmmon.h gmsmall.h; do git -C "$gm" show "$rev:hardware/pocket-player/src/$f" > "$out/$f"; done
git -C "$gm" show "$rev:LICENSE" > "$out/LICENSE"
printf 'GameMaker-SandboxEdition %s\nhttps://github.com/lewismoten/GameMaker-SandboxEdition/tree/%s/hardware/pocket-player/src\n' "$rev" "$rev" > "$out/PINNED"
echo "third_party/gmcore is GameMaker-SandboxEdition $rev"
