"""Callers of the given functions in a `gprof -b` call graph, in calls per frame.

usage: python3 perf_callers.py <gprof.txt> <frames> <min calls per frame> <function> [function...]
With the functions "LOOKUPS" the table lookups by key are added per caller (tget_at tnum_at tstr_at ttab_at tbool_at
tobj_at tset_at tget tset tgetv tnext): one line per game function.
"""
import re
import sys
from collections import defaultdict

LOOKUPS = 'tget_at tnum_at tstr_at ttab_at tbool_at tobj_at tset_at tget tset tgetv tsetv tnext tpath'.split()


def blocks(path):
    """{function: [(caller, calls)]} from the call graph section."""
    text = open(path).read()
    graph = re.sub(r' <cycle \d+>', '', text[text.index('Call graph'):])
    res = {}
    for block in graph.split('-----------------------------------------------\n'):
        lines = block.strip('\n').split('\n')
        prim = next((i for i, l in enumerate(lines) if l.startswith('[')), None)
        if prim is None:
            continue
        m = re.search(r'\s(\S+) \[\d+\]$', lines[prim])
        if not m:
            continue
        callers = []
        for l in lines[:prim]:
            m2 = re.search(r'(\d+)/\d+\s+(\S+) \[\d+\]$', l)
            if m2:
                callers.append((m2.group(2), int(m2.group(1))))
        res.setdefault(m.group(1), []).extend(callers)
    return res


def main():
    path, frames, least = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
    names = sys.argv[4:]
    g = blocks(path)
    if names == ['LOOKUPS']:
        total = defaultdict(lambda: defaultdict(int))
        for f in LOOKUPS:
            for caller, n in g.get(f, []):
                if caller not in LOOKUPS:
                    total[caller][f] += n
        rows = sorted(total.items(), key=lambda kv: -sum(kv[1].values()))
        print('all: %.0f a frame' % (sum(sum(v.values()) for v in total.values()) / frames))
        for caller, by in rows:
            s = sum(by.values()) / frames
            if s >= least:
                print('%9.1f  %-40s %s' % (s, caller, ' '.join('%s=%.0f' % (k.replace('_at', ''), v / frames) for k, v in sorted(by.items(), key=lambda kv: -kv[1]))))
        return
    for f in names:
        callers = sorted(g.get(f, []), key=lambda c: -c[1])
        print('%s: %.1f a frame' % (f, sum(n for _, n in callers) / frames))
        for caller, n in callers:
            if n / frames >= least:
                print('%9.1f  %s' % (n / frames, caller))


main()
