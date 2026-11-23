#!/bin/bash
# tools/build_blinds.sh: build both binaries and copy them to /root/bl-bin for tools/both_blinds.sh
N=${BALATRO_NATIVE:-$(cd "$(dirname "$0")/.." && pwd)}
bash $N/build.sh link asan && bash $N/build.sh link
ls ${BALATRO_BUILD:-$HOME/balatro-native}/err/
mkdir -p /root/bl-bin && cp ${BALATRO_BUILD:-$HOME/balatro-native}/balatro-asan ${BALATRO_BUILD:-$HOME/balatro-native}/balatro /root/bl-bin/ && echo copied
