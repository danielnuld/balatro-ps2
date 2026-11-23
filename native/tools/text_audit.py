#!/usr/bin/env python3
# Lists the string literals of src/*.c and include/*.h that look like text for a person (a space, a sentence, a
# capitalised word) rather than a key, so that the game's wording typed into C is noticed: the game's text must come
# from data/*.dat (tools/dump_data.sh), never from this tree.
#   python3 tools/text_audit.py [game dir]      one line per literal: file:line <TAB> where <TAB> literal
# With the game's directory, `where` says whether the same literal is in the game's Lua ("lua") or not ("own").
import glob, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"((?:[^"\\\n]|\\.)*)"|\'(?:[^\'\\\n]|\\.)+\'', re.S)
LUA_TOKEN = re.compile(r'--\[\[.*?\]\]|--[^\n]*|"((?:[^"\\\n]|\\.)*)"|\'((?:[^\'\\\n]|\\.)*)\'', re.S)
ESC = re.compile(r'\\(.)')


def unescape(s):
    return ESC.sub(lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), s)


def literals(path):
    src = open(path, encoding='utf-8', errors='replace').read()
    # the lines of the preprocessor that name files are not text
    for m in TOKEN.finditer(src):
        if m.group(1) is None:
            continue
        line = src.count('\n', 0, m.start()) + 1
        start = src.rfind('\n', 0, m.start()) + 1
        if src[start:m.start()].lstrip().startswith('#include'):
            continue
        yield line, unescape(m.group(1))


def looks_like_text(s):
    if len(s) < 2 or not re.search(r'[A-Za-z]{2}', s):
        return False
    if re.fullmatch(r'[A-Za-z0-9_.:/\-]+', s):
        # one token: a key unless it is a capitalised word ("Seed", "Production") and not an ALL_CAPS or snake id
        return bool(re.fullmatch(r'[A-Z][a-z]+[a-z0-9]*', s))
    return True


def main():
    lua = None
    if len(sys.argv) > 1:
        lua = set()
        for path in glob.glob(os.path.join(sys.argv[1], '**', '*.lua'), recursive=True):
            if os.sep + 'localization' + os.sep in path:
                continue
            for m in LUA_TOKEN.finditer(open(path, encoding='utf-8', errors='replace').read()):
                for g in m.groups():
                    if g is not None:
                        lua.add(unescape(g))
    files = sorted(glob.glob(os.path.join(ROOT, 'src', '*.c')) + glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True))
    for path in files:
        for line, s in literals(path):
            if looks_like_text(s):
                where = '' if lua is None else ('lua' if s in lua else 'own')
                print('%s:%d\t%s\t%s' % (os.path.relpath(path, ROOT).replace(os.sep, '/'), line, where, s.replace('\n', '\\n')))


if __name__ == '__main__':
    main()
