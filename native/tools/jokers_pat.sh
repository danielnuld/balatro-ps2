#!/bin/bash
# tools/jokers_pat.sh <tag> <pattern> [asan|opt]: tools/jokers.sh for every joker with CASE=<pattern>, % being the key
# (e.g. bp 'j_blueprint,%' or bs '%,j_brainstorm'); other variables (SEED, ACT<i>...) pass through.
# Results in /root/jk/pat-<tag>-<mode>.txt.
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
tag=$1; pat=$2; mode=${3:-opt}
cp ${BALATRO_BUILD:-$HOME/balatro-native}/balatro$([ "$mode" = asan ] && echo -asan) /root/jk/bin-$mode 2>/dev/null; export JKBIN=/root/jk/bin-$mode
for k in $(cat /root/jk/keys.txt); do CASE=${pat//%/$k} bash $N/tools/jokers.sh $tag-$k $mode; done 2>&1 | tee /root/jk/pat-$tag-$mode.txt | grep -a -c '^SAME'
grep -a '^DIFF' /root/jk/pat-$tag-$mode.txt | awk '{print $2}' | tr '\n' ' '
