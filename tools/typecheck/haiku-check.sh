#!/bin/sh
# haiku-check.sh — type-checks Folio's Haiku-API sources against Haiku's real
# system headers on a Linux host (g++ -fsyntax-only). Catches wrong API names,
# signatures and types; it does not link or run anything.
#
#   sh tools/typecheck/haiku-check.sh [CACHE_DIR]     (default: ./.haiku-headers)
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=$(mkdir -p "${1:-.haiku-headers}" && cd "${1:-.haiku-headers}" && pwd)

if [ ! -d "$CACHE/haiku/headers/os" ]; then
	git clone -q --depth 1 --filter=blob:none --sparse \
		https://github.com/haiku/haiku.git "$CACHE/haiku"
	git -C "$CACHE/haiku" sparse-checkout set headers/os headers/posix \
		headers/config headers/build/config_headers
fi
H=$CACHE/haiku/headers

# Haiku's headers expect Haiku's libc; these few types are all that's missing
# when they sit on top of glibc.
cat > "$CACHE/shim.h" <<'EOF'
#include <config/types.h>
#include <stddef.h>
typedef unsigned long addr_t;
EOF

INC="-I$H"
for d in "$H"/os "$H"/os/*/; do INC="$INC -I$d"; done

status=0
for f in App.cpp MainWindow.cpp NoteView.cpp Widgets.cpp Vault.cpp; do
	out=$(g++ -std=c++17 -Wall -Wno-cpp -Wno-multichar -fsyntax-only $INC \
		-include "$CACHE/shim.h" "$ROOT/src/$f" 2>&1 \
		| grep -E "^[^ ].*: (error|warning): " | grep -v "^$H/" | grep -v "redefined" || true)
	if [ -n "$out" ]; then echo "$out"; status=1; else echo "ok  $f"; fi
done
exit $status
