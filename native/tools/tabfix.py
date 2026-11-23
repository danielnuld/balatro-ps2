"""Wrap the keys of every TAB(...) call in S(): TAB("k", v, "k2", v2) -> TAB(S("k"), v, S("k2"), v2).

usage: python3 tabfix.py <src dir> <out dir>
For the PS2 build (-DTAB_ARRAYS, see include/core.h): its compiler dies on TAB as a call with dozens of struct
variable arguments, so there TAB is an array of Vals and every key has to be one.
"""
import re
import sys
from pathlib import Path


def skip(s, i):
    """The index after the string, character or comment that starts at i, or i itself."""
    c = s[i]
    if c in '"\'':
        j = i + 1
        while s[j] != c:
            j += 2 if s[j] == '\\' else 1
        return j + 1
    if s.startswith('//', i):
        j = s.find('\n', i)
        return len(s) if j < 0 else j
    if s.startswith('/*', i):
        return s.index('*/', i) + 2
    return i


def fix(s):
    out, i = [], 0
    for m in re.finditer(r'\bTAB\s*\(', s):
        if m.start() < i:
            continue  # inside a TAB already rewritten (its arguments were fixed recursively)
        # not in a string or a comment: scan from the last position
        j, inside = i, False
        while j < m.start():
            k = skip(s, j)
            if k > m.start():
                inside = True
                break
            j = k if k != j else j + 1
        if inside:
            continue
        args, depth, start, j = [], 1, m.end(), m.end()
        while depth:
            k = skip(s, j)
            if k != j:
                j = k
                continue
            c = s[j]
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
                if not depth:
                    break
            elif c == ',' and depth == 1:
                args.append(s[start:j])
                start = j + 1
            j += 1
        args.append(s[start:j])
        fixed = []
        for n, a in enumerate(args):
            a = fix(a)
            if n % 2 == 0:
                lead = len(a) - len(a.lstrip())
                a = a[:lead] + 'S(' + a.strip() + ')' + a[len(a.rstrip()):]
            fixed.append(a)
        out.append(s[i:m.end()] + ','.join(fixed) + ')')
        i = j + 1
    out.append(s[i:])
    return ''.join(out)


def main(src, dst):
    Path(dst).mkdir(parents=True, exist_ok=True)
    for f in Path(src).glob('*.c'):
        (Path(dst) / f.name).write_text(fix(f.read_text(encoding='utf-8')), encoding='utf-8')


if __name__ == '__main__':
    assert fix('x = TAB("a", N(1), "b", TAB("c", S("TAB(,)"), k, v)); // TAB("no", 1)') == \
        'x = TAB(S("a"), N(1), S("b"), TAB(S("c"), S("TAB(,)"), S(k), v)); // TAB("no", 1)', fix('x = TAB("a", N(1), "b", TAB("c", S("TAB(,)"), k, v)); // TAB("no", 1)')
    main(sys.argv[1], sys.argv[2])
