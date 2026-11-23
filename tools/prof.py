"""Read the EE sampling profile (-DPROF, see runtime/src/main.c): time per function.

usage: python tools/prof.py prof.bin symbols.txt [lines]
  symbols.txt: mips64r5900el-ps2-elf-nm -n <elf>   (the ELF that wrote prof.bin)
"""
import bisect
import struct
import sys


def main(prof, symbols, lines=40):
    syms = []
    for line in open(symbols):
        parts = line.split()
        if len(parts) == 3 and parts[1] in "tT":
            syms.append((int(parts[0], 16), parts[2]))
    syms.sort()
    start = dict((name, addr) for addr, name in syms)["_ftext"]
    addrs = [a for a, _ in syms]
    data = open(prof, "rb").read()
    total, per = 0, {}
    for i, n in enumerate(struct.unpack("<%dH" % (len(data) // 2), data)):
        if n:
            name = syms[bisect.bisect_right(addrs, start + i * 16 + 8) - 1][1] if i else "(outside the text)"
            per[name] = per.get(name, 0) + n
            total += n
    print(f"{total} samples")
    for name, n in sorted(per.items(), key=lambda kv: -kv[1])[:int(lines)]:
        print(f"{n / total * 100:6.2f} %  {name}")


if __name__ == "__main__":
    main(*sys.argv[1:])
