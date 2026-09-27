#!/bin/sh
# Check one function in a C file against the original, without touching build/.
# Runs inside the container:   tools/check.sh func_80012345 src/game/80012345.c
# Prints the number of non-matching instructions per compiler (0 = match).
# Relocated fields (symbol addresses) are masked; `make` verifies those.
set -e
# Compile the C side of NEEDS_RODATA / NON_MATCHING guards so that C is what gets checked.
export CPPFLAGS_EXTRA="${CPPFLAGS_EXTRA:--DNEEDS_RODATA -DNEEDS_SDATA -DNON_MATCHING}"
PROBE_GCCS=${PROBE_GCCS:-2.8.1-psx,2.8.0-psx,2.7.2-cdk} PROBE_OPTS=${PROBE_OPTS:--O2} \
PROBE_ASPSX=${PROBE_ASPSX:-2.79} exec tools/probe.py "$@"
