#!/usr/bin/env python3
import json,sys
from pathlib import Path
if len(sys.argv)!=3:
    print('Gebruik: python3 tools/add_language.py es Español'); raise SystemExit(2)
code,label=sys.argv[1].lower(),sys.argv[2]
ROOT=Path(__file__).resolve().parents[1]
LANG=ROOT/'components/web_server/web/lang'
catp=LANG/'languages.json'; cat=json.loads(catp.read_text(encoding='utf-8'))
if code in cat: raise SystemExit(f'Taal {code} bestaat al')
base=json.loads((LANG/'nl.json').read_text(encoding='utf-8'))
(LANG/f'{code}.json').write_text(json.dumps(base,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
cat[code]=label; catp.write_text(json.dumps(cat,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'Aangemaakt: {code}.json en toegevoegd aan languages.json')
