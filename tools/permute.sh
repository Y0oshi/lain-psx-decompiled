#!/bin/bash
# Run decomp-permuter on one NON_MATCHING function. Runs in the container:
#   tools/docker.sh tools/permute.sh func_80012345 src/game/80012300.c [permuter args]
#   tools/docker.sh env PERMUTE_TIME=45m tools/permute.sh ...   (docker.sh passes no env)
# Imports the function into build/permuter/<func>/ (base.c, target.o,
# compile.sh -> tools/cc.sh) and permutes for $PERMUTE_TIME (default 20m) with
# $PERMUTE_JOBS threads (default 4). Better candidates land in
# build/permuter/<func>/output-<score>-<n>/source.c. The permuter's score is
# not check.sh's, so the outputs are re-ranked by check.sh diffs at the end;
# always re-check a candidate you port back into src/.
#   PERMUTE_IMPORT=0  keep an existing work dir (e.g. after editing base.c
#                     by hand, or to add PERM_ macros); default re-imports.
#   PERMUTE_PRUNE=0   don't prune unused declarations from base.c.
set -e
func=$1 src=$2; shift 2
dir=build/permuter/$func
if [ "${PERMUTE_IMPORT:-1}" != 0 ] || [ ! -f "$dir/base.c" ]; then
    rm -rf "$dir"
    python3 tools/permute_import.py "$func" "$src" "$dir" \
        $([ "${PERMUTE_PRUNE:-1}" = 0 ] && echo --no-prune)
fi
timeout -s INT -k 30s "${PERMUTE_TIME:-20m}" \
    python3 /opt/permuter/permuter.py -j"${PERMUTE_JOBS:-4}" --best-only "$@" "$dir" || true
echo "check.sh diffs of the outputs:"
python3 tools/permute_import.py --rank "$func" "$dir"
