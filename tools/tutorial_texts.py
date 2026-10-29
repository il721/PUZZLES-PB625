"""Reads the walkthrough texts back out of native/puzzlebook/tutorial_text.c (for the UI test script).
load() -> {'REBUS': [(en, ru, de), ...], 'DIGIT': ..., 'DOMINO': ..., 'LAB': ..., 'SQUARE': ..., 'KEYWORD': ...}"""
import os, re

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'native', 'puzzlebook', 'tutorial_text.c')
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s):
    return s.replace('\\"', '"').replace('\\\\', '\\')


def load():
    src = open(PATH, encoding='utf-8').read()
    out = {}
    for m in re.finditer(r'static const Str T_(\w+)\[\d+\] = \{(.*?)\n\};', src, re.S):
        steps, body = [], m.group(2)
        for e in re.finditer(r'S\(((?:"(?:[^"\\]|\\.)*"|[\s,])*)\)', body):
            langs, cur, sep = [], '', False
            for tok in re.finditer(r'"((?:[^"\\]|\\.)*)"|,', e.group(1)):
                if tok.group(0) == ',':
                    langs.append(cur)
                    cur = ''
                else:
                    cur += unescape(tok.group(1))
            langs.append(cur)
            assert len(langs) == 3, langs
            steps.append(tuple(langs))
        out[m.group(1)] = steps
    return out


if __name__ == '__main__':
    d = load()
    print({k: len(v) for k, v in d.items()})
    print(d['KEYWORD'][9][0])
