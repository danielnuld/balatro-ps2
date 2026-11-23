"""The time arguments of the event helpers as num: float literals (0.2f) and (float) casts in those argument
positions become doubles, at every call in src/. Run once; kept to re-run after a merge: it changes nothing
when there is nothing left.  usage: python tools/delay_num.py [--check]   (from native/)
"""
import glob
import re
import sys

# function -> the 0-based positions of its arguments that are seconds (or factors of seconds)
ARGS = {
    'delay': [0], 'draw_card': [6], 'ease_value': [6], 'ease_value_f': [5], 'ease_value_n': [5], 'ease_colour': [2],
    'ease_value_add': [6], 'play_area_status_text': [2], 'juice_card_until': [3], 'Card_explode': [2],
    'Card_start_dissolve': [3], 'Card_start_materialize': [3], 'FUNCS_end_consumeable': [1], 'FUNCS_wipe_on': [2],
    'Particles_fade': [1], 'fade_ctx_new': [1], 'add_after': [0],
}
CALL = re.compile(r'\b(' + '|'.join(ARGS) + r')\s*\(')


def split_args(s, i):
    """The arguments of the call whose '(' is just before i: [(start, end)], and the index after ')'."""
    args, depth, start = [], 1, i
    while depth:
        c = s[i]
        if c in '"\'':
            i += 1
            while s[i] != c:
                i += 2 if s[i] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if not depth:
                break
        elif c == ',' and depth == 1:
            args.append((start, i))
            start = i + 1
        i += 1
    args.append((start, i))
    return args, i + 1


def fix_arg(a):
    b = re.sub(r'(?<![\w.])(\d+\.\d*|\.\d+|\d+)[fF]\b', r'\1', a)       # 0.2f -> 0.2
    b = re.sub(r'^(\s*)\(float\)\s*', r'\1', b)                        # (float)(0.2 * x) -> (0.2 * x)
    return b


def fix(s):
    out, pos, n = [], 0, 0
    for m in CALL.finditer(s):
        if m.start() < pos:
            continue
        args, end = split_args(s, m.end())
        text = s[m.end():args[-1][1]]
        if re.match(r'\s*(float|num|const|Card|CardArea|Table|UIElement|Particles|EventCfg|void)\b', text):
            continue  # a prototype or a definition
        new, last = [], m.end()
        for k, (a, b) in enumerate(args):
            arg = s[a:b]
            if k in ARGS[m.group(1)]:
                fixed = fix_arg(arg)
                n += fixed != arg
                arg = fixed
            new.append(arg)
        out.append(s[pos:m.end()] + ','.join(new) + ')')
        pos = end
    out.append(s[pos:])
    return ''.join(out), n


def main():
    total = 0
    for p in sorted(glob.glob('src/*.c')):
        s = open(p, encoding='utf-8', newline='').read()
        t, n = fix(s)
        if n:
            print(p, n)
            total += n
            if '--check' not in sys.argv:
                open(p, 'w', encoding='utf-8', newline='').write(t)
    print('arguments changed:', total)


main()
