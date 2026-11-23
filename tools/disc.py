"""A PS2 disc image of the port: the ELF, SYSTEM.CNF and GAME.PAK, the game's files in one (runtime/src/pack.c).

usage: python tools/disc.py [elf, default <run dir>/balatro-disc.elf] [iso, default <run dir>/balatro.iso]
       python tools/disc.py pack [file, default build/GAME.PAK]      only the pack (the host tests read it)

The pack holds what the runtime reads of build/game: the Lua, files.txt, the .tex of tools/textures.py and
tools/font.py and the .snd of tools/sounds.py. Nothing of the game goes in git: the pack stays in build/ and the
ISO in the run directory (%LOCALAPPDATA%/balatro-ps2, where PCSX2 runs from).
The ELF is built with XF=-DDISC and goes in as BLTR_000.01: launchers take the boot file's name as the game's
serial (not a Sony prefix). The ISO is made by mkisofs in WSL on the WSL disk (not through a Windows drive).
"""
import io
import os
import struct
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOOT = "BLTR_000.01"
KEEP = {".lua", ".txt", ".tex", ".snd", ".jkr"}
SECTOR = 2048


def pack(out):
    game = ROOT / "build/game"
    files = sorted((f.relative_to(game).as_posix().encode(), f) for f in game.rglob("*") if f.is_file() and f.suffix in KEEP)
    names = [n for n, _ in files]
    assert names == sorted(names) and files, "no game files: run tools/extract.py, textures.py, font.py and sounds.py"
    at = 8 + sum(10 + len(n) for n in names)
    index, starts = [b"BPAK", struct.pack("<I", len(files))], []
    for name, f in files:
        at = -(-at // SECTOR) * SECTOR  # every file starts on a sector
        starts.append(at)
        index.append(struct.pack("<IIH", at, f.stat().st_size, len(name)) + name)
        at += f.stat().st_size
    with open(out, "wb") as o:
        o.write(b"".join(index))
        for start, (_, f) in zip(starts, files):
            o.write(b"\0" * (start - o.tell()))
            o.write(f.read_bytes())
    print(f"{out}: {len(files)} files, {Path(out).stat().st_size / 2**20:.1f} MB")


def main():
    if sys.argv[1:2] == ["pack"]:
        return pack(sys.argv[2] if len(sys.argv) > 2 else ROOT / "build/GAME.PAK")
    run = Path(os.environ.get("BALATRO_RUN") or Path(os.environ["LOCALAPPDATA"]) / "balatro-ps2")
    elf = Path(sys.argv[1]) if len(sys.argv) > 1 else run / "balatro-disc.elf"
    iso = Path(sys.argv[2]) if len(sys.argv) > 2 else run / "balatro.iso"
    pack(ROOT / "build/GAME.PAK")
    cnf = f"BOOT2 = cdrom0:\\{BOOT};1\r\nVER = 1.00\r\nVMODE = NTSC\r\n".encode()
    work = "/root/balatro-disc"
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    subprocess.run(["wsl.exe", "-e", "bash", "-c", f"rm -rf {work} && mkdir -p {work}"], check=True, env=env)
    untar = subprocess.Popen(["wsl.exe", "-e", "tar", "-x", "-C", work], stdin=subprocess.PIPE, env=env)
    with tarfile.open(fileobj=untar.stdin, mode="w|") as tar:  # the disc's three files, straight to the WSL disk
        for name, data in (("GAME.PAK", (ROOT / "build/GAME.PAK").read_bytes()), (BOOT, elf.read_bytes()), ("SYSTEM.CNF", cnf)):
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
    untar.stdin.close()
    assert untar.wait() == 0, "could not copy the disc's files to WSL"
    subprocess.run(["wsl.exe", "-e", "mkisofs", "-quiet", "-iso-level", "1", "-V", "BALATRO", "-sysid", "PLAYSTATION",
                    "-o", "/root/balatro.iso", work], check=True, env=env)
    with open(iso, "wb") as o:
        subprocess.run(["wsl.exe", "-e", "cat", "/root/balatro.iso"], stdout=o, check=True, env=env)
    print(f"{iso}: {iso.stat().st_size / 2**20:.1f} MB")


if __name__ == "__main__":
    main()
