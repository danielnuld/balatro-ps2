#!/bin/bash
# Run an ELF of the run directory in PCSX2 (minimized, in the background) until it prints DONE or ERROR, then
# show its BOOT / PERF / ERROR lines. The whole log is kept as <elf>.log beside it.
# usage (Git Bash): tools/pcsx2.sh [balatro.elf | balatro.iso] [timeout seconds]
# The ELF reads host:game: PCSX2 needs HostFs = true, and the run directory gets a copy of build/game once
# (delete <run dir>/game after tools/extract.py runs again).
cd "$(dirname "$0")/.."
RUN=${BALATRO_RUN:-$(cygpath -u "$LOCALAPPDATA")/balatro-ps2}
ELF=${1:-balatro.elf}
LOG=~/Documents/PCSX2/logs/emulog.txt
[ -d "$RUN/game" ] || cp -r build/game "$RUN/game"
stop() { powershell.exe -NoProfile -Command "Get-Process pcsx2* -ErrorAction SilentlyContinue | Stop-Process -Force" >/dev/null 2>&1; }
# One PCSX2 at a time: a lock directory, PCSX2_LOCK to share it with other projects' scripts.
LOCK=${PCSX2_LOCK:-/tmp/pcsx2.lock}
until mkdir $LOCK 2>/dev/null; do sleep 20; done
trap 'stop; rmdir $LOCK' EXIT
stop
rm -f $LOG
HOW="'-elf'"; case $ELF in *.iso) HOW="'-fastboot','--'";; esac  # a disc image boots as a disc
powershell.exe -NoProfile -Command "Start-Process 'C:\Program Files\PCSX2\pcsx2-qt.exe' -ArgumentList $HOW,'$(cygpath -w "$RUN/$ELF")' -WindowStyle Minimized"
SHOTS=0
for _ in $(seq $(( ${2:-600} / 2 ))); do
  sleep 2
  grep -aqE "] (DONE|ERROR|BOOT ERROR)" $LOG 2>/dev/null && break
  # a new SHOT line: the frame is held on screen; PCSX2's screenshot key (F8) saves Documents/PCSX2/snaps/*.png
  n=$(grep -acE "] SHOT" $LOG 2>/dev/null)
  if [ "${n:-0}" -gt "$SHOTS" ]; then
    SHOTS=$n
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w tools/shot.ps1)"
  fi
done
cp $LOG "$RUN/$ELF.log"
grep -aE "] (BOOT|PERF|SECT|SHOWN|ERROR|DONE|	)" "$RUN/$ELF.log" || { echo "no output; the log's end:"; tail -5 "$RUN/$ELF.log"; }
