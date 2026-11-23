"""A PS2 disc image of the native port: the repo's tools/disc.py with the port's data files in the pack.

usage: python tools/disc_native.py <repo> <elf built with XF=-DDISC> <iso>
The data files (native/data/*.dat) are copied to <repo>/build/game/data (build/ is not in git) so the pack takes them.
"""
import shutil
import sys
from pathlib import Path

repo, elf, iso = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
data = repo / "build/game/data"
data.mkdir(exist_ok=True)
for f in (Path(__file__).resolve().parent.parent / "data").glob("*.dat"):
    shutil.copy(f, data / f.name)
sys.path.insert(0, str(repo / "tools"))
import disc  # noqa: E402

disc.KEEP |= {".dat"}
sys.argv = ["disc.py", elf, iso]
disc.main()
