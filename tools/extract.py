"""Extract the game from your own Balatro.exe (love.exe with a zip appended) into build/game/.

usage: python tools/extract.py <Balatro.exe> [out_dir]
Nothing extracted may be committed: build/ is ignored.
"""
import hashlib
import sys
import zipfile
from pathlib import Path


def main(exe, out="build/game"):
    print("sha1", hashlib.sha1(Path(exe).read_bytes()).hexdigest())
    with zipfile.ZipFile(exe) as z:  # zipfile finds the archive behind the exe stub
        z.extractall(out)
        names = z.namelist()
        if "version.jkr" in names:
            print("version", z.read("version.jkr").decode().splitlines()[0])
        # the runtime's file list (love.filesystem.getInfo / getDirectoryItems): "<size> <path>" per file
        listing = "".join(f"{i.file_size} {i.filename}\n" for i in z.infolist() if not i.is_dir())
        Path(out, "files.txt").write_bytes(listing.encode())
        print(len(names), "entries ->", out)
    # ponytail: no list of tested versions yet; reject unknown sha1s here once one version is known to work


if __name__ == "__main__":
    main(*sys.argv[1:3])
