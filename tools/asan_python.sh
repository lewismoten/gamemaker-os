#!/bin/sh
# SPDX-License-Identifier: MIT; prints a Python that can be started with the
# AddressSanitizer runtime preloaded (make test-asan). macOS refuses a
# sanitizer in a hardened-runtime binary (the python.org installer's), and a
# framework build's bin/python3 re-executes Python.app, which drops
# DYLD_INSERT_LIBRARIES; so this finds that inner executable, unhardened.
for p in "$@" python3 /opt/homebrew/bin/python3 /usr/local/bin/python3; do
  command -v "$p" >/dev/null 2>&1 || continue
  exe=$("$p" -c 'import os, sys; q = os.path.join(sys.prefix, "Resources/Python.app/Contents/MacOS/Python"); print(q if os.path.exists(q) else os.path.realpath(sys.executable))' 2>/dev/null) || continue
  if [ "$(uname)" != Darwin ] || ! codesign -dv "$exe" 2>&1 | grep -q 'flags=.*runtime'; then echo "$exe"; exit 0; fi
done
echo "asan_python.sh: no Python without the hardened runtime (install one with Homebrew)" >&2
exit 1
