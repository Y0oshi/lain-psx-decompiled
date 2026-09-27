#!/bin/sh
# Run m2c on one function from the split asm: tools/m2c.sh func_80012345 [m2c args]
set -e
f=$1; shift
src=$(grep -rl "^glabel $f\$" asm | head -1)
tmp=$(mktemp)
awk -v f="$f" '$0 ~ "^glabel "f"$"{p=1} p{print} $0 ~ "^endlabel "f"$"{p=0}' "$src" > "$tmp"
m2c --target mips-gcc-c --valid-syntax "$@" "$tmp"
rm -f "$tmp"
