#!/bin/bash
# tools/video_pick_test.sh <elf in the run directory> <step>...   (Git Bash, as the repo's tools/pcsx2.sh)
# The mode screen (src/love_video.c) driven in PCSX2 with the keyboard: the repo's tools/pcsx2.sh only watches the
# log, so this starts PCSX2 the same way, under the same lock, and then does the steps:
#   wait:<text>   until the log has had that text once more than at the last wait for it (60 s at most)
#   key:down | key:up | key:cross | key:circle     a pad button (PCSX2.ini's keys: arrows, K, L), held 150 ms
#   shot          PCSX2's screenshot key (Documents/PCSX2/snaps)
#   sleep:<seconds>
# The log is left as <elf>.log in the run directory; its VIDEO lines are printed.
REPO=${BALATRO_REPO:-$(cd "$(dirname "$0")/../.." && pwd)}
RUN=$(cygpath -u "$LOCALAPPDATA")/balatro-ps2
ELF=$1; shift
LOG=~/Documents/PCSX2/logs/emulog.txt
key() { powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w $REPO/tools/shot.ps1)" "$@"; }
stop() { powershell.exe -NoProfile -Command "Get-Process pcsx2* -ErrorAction SilentlyContinue | Stop-Process -Force" >/dev/null 2>&1; }
LOCK=${PCSX2_LOCK:-/tmp/pcsx2.lock}
until mkdir $LOCK 2>/dev/null; do sleep 20; done
trap 'stop; rmdir $LOCK' EXIT
stop
rm -f $LOG
powershell.exe -NoProfile -Command "Start-Process 'C:\Program Files\PCSX2\pcsx2-qt.exe' -ArgumentList '-elf','$(cygpath -w "$RUN/$ELF")' -WindowStyle Minimized"
declare -A seen
for step in "$@"; do
  case $step in
    wait:*) t=${step#wait:}; n=$(( ${seen[$t]:-0} + 1 )); seen[$t]=$n
            for _ in $(seq 120); do [ "$(grep -ac "$t" $LOG 2>/dev/null || true)" -ge $n ] 2>/dev/null && break; sleep 0.5; done
            [ "$(grep -ac "$t" $LOG 2>/dev/null || true)" -ge $n ] 2>/dev/null || echo "NOT SEEN: $t ($n)";;
    key:down) key 0x28 0x150 150;;
    key:up) key 0x26 0x148 150;;
    key:cross) key 0x4B 0x25 150;;
    key:circle) key 0x4C 0x26 150;;
    shot) key;;
    sleep:*) sleep ${step#sleep:};;
  esac
  echo "$step"
done
cp $LOG "$RUN/$ELF.log"
grep -aE "] (VIDEO|ERROR|BOOT (loaded|platform))" "$RUN/$ELF.log"
