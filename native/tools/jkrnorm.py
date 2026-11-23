# tools/jkrnorm.py <file.jkr>: a save file of the game (STR_PACK text, deflated, with or without the card's 12 byte
# header) as text with the keys sorted, one top-level key per line: to diff the oracle's files with the port's.
import sys, zlib, re
d = open(sys.argv[1], 'rb').read()
if d[:4] == b'BLT1': d = d[12:]
try: s = zlib.decompress(d, -15).decode('latin-1')
except Exception: s = d.decode('latin-1')
s = s[len('return '):]
pos = 0
BS = chr(92)

def string():
    global pos
    j = pos + 1
    while s[j] != '"': j += 2 if s[j] == BS else 1
    r = s[pos + 1:j]
    pos = j + 1
    return r

def val():
    global pos
    if s[pos] == '{':
        pos += 1
        t = {}
        while s[pos] != '}':
            assert s[pos] == '[', s[pos:pos + 30]
            pos += 1
            if s[pos] == '"': k = string()
            else:
                m = re.compile(r'[^\]]+').match(s, pos)
                k = m.group()
                pos = m.end()
            assert s[pos:pos + 2] == ']=', s[pos:pos + 30]
            pos += 2
            t[k] = val()
            assert s[pos] == ','
            pos += 1
        pos += 1
        return t
    if s[pos] == '"': return string()
    m = re.compile(r'[^,}]+').match(s, pos)
    pos = m.end()
    return m.group()

def out(v):
    if isinstance(v, dict): return '{' + ','.join(k + '=' + out(v[k]) for k in sorted(v)) + '}'
    return v

t = val()
for k in sorted(t): print(k, out(t[k]))
