#!/bin/sh
# SPDX-License-Identifier: MIT
# Copy the sandbox-kernel SDK (its sdk/ folder, MIT) into sdk/ and record the
# kernel commit it came from in sdk/PINNED. Usage: scripts/vendor_sdk.sh <kernel checkout> [commit]
set -eu
kernel=$1; commit=${2:-HEAD}
here=$(cd "$(dirname "$0")/.." && pwd)
rev=$(git -C "$kernel" rev-parse "$commit")
rm -rf "$here/sdk"; mkdir -p "$here/sdk"
git -C "$kernel" archive "$rev" sdk | tar -x -C "$here"
printf 'sandbox-kernel %s\nhttps://github.com/lewismoten/sandbox-kernel/tree/%s/sdk\n' "$rev" "$rev" > "$here/sdk/PINNED"
echo "sdk/ is sandbox-kernel $rev"
