#!/bin/sh
# [CHOSEN, not recovered] GPU equivalence over a list of effects (two workers). Entries come
# from `nativefx structure` with NATIVEFX_SAVE_ENTRIES/NATIVEFX_SAVE_REFS.
# Usage: gpu-all.sh LIST.tsv NATIVE_DIR REF_DIR OUT.tsv
set -u
cd "$(dirname "$0")/../../.."
list=$1 nat=$2 ref=$3 out=$4
rm -f "$out" "$out".w0 "$out".w1 "$out".err
awk 'NR % 2 == 1 {print $1 > "'"$out"'.l0"} NR % 2 == 0 {print $1 > "'"$out"'.l1"}' "$list"
for w in 0 1; do
  (while read id; do
     out/build/nativefx/tool/gpucheck "$nat/$id.entry" "$ref/$id.entry" "$out.w$w" "$id" > /dev/null 2>> "$out.err" || echo "$id crashed" >> "$out.err"
   done < "$out.l$w") &
done
wait
cat "$out".w0 "$out".w1 > "$out"
rm -f "$out".w0 "$out".w1 "$out".l0 "$out".l1
