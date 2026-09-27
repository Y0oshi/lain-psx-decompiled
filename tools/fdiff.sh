#!/bin/sh
# Side-by-side diff of one function: original asm vs. compiled C.
#   tools/fdiff.sh func_80012345 file.c      (GCC_VER/OPTFLAGS/ASPSX_VER env honoured)
set -e
# Compile the C side of NEEDS_RODATA / NON_MATCHING guards so that C is what gets checked.
export CPPFLAGS_EXTRA="${CPPFLAGS_EXTRA:--DNEEDS_RODATA -DNEEDS_SDATA -DNON_MATCHING}"
f=$1; c=$2
tmp=$(mktemp -d)
tools/cc.sh "$c" "$tmp/t.o"
mips-linux-gnu-objdump -d -r --no-show-raw-insn "$tmp/t.o" \
  | awk -v f="$f" '$0 ~ "<"f">:"{p=1;next} p&&/^$/{exit} p' \
  | grep -v "R_MIPS" | sed -E 's/^ *[0-9a-f]+:\t//; s/\t+/ /g; s/<[^>]*>//' > "$tmp/got"
src=$(grep -rl "^glabel $f\$" asm | head -1)
awk -v f="$f" '$0 ~ "^glabel "f"$"{p=1;next} $0 ~ "^endlabel "f"$"{p=0} p' "$src" \
  | grep -E '^\s+/\*' | sed -E 's|/\* [0-9A-F]+ [0-9A-F]+ [0-9A-F]+ \*/ *||; s/\$//g; s/ +/ /g; s/^ //' > "$tmp/want"
diff -y -W 110 "$tmp/want" "$tmp/got" || true
rm -rf "$tmp"
