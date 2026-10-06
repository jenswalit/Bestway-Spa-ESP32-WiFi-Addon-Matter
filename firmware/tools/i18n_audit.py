#!/usr/bin/env python3
import json,re
from pathlib import Path
from bs4 import BeautifulSoup
ROOT=Path(__file__).resolve().parents[1]
WEB=ROOT/'components/web_server/web'
LANG=WEB/'lang'
langs=json.loads((LANG/'languages.json').read_text(encoding='utf-8'))
base=json.loads((LANG/'nl.json').read_text(encoding='utf-8'))
keys=set()
app=(WEB/'js/app.js').read_text(encoding='utf-8')
keys.update(v for _,v in re.findall(r'\btr\(\s*(["\'])(.*?)\1\s*\)',app,re.S))
soup=BeautifulSoup((WEB/'index.html').read_text(encoding='utf-8'),'html.parser')
for n in soup.find_all(string=True):
    if n.parent.name in ('script','style'): continue
    t=' '.join(n.strip().split())
    if t and any(c.isalpha() for c in t): keys.add(t)
for el in soup.find_all(True):
    if el.get('data-i18n'): keys.add(el['data-i18n'])
    for attr in ('title','aria-label','placeholder'):
        k=el.get(f'data-i18n-{attr}')
        if k: keys.add(k)
        elif el.get(attr) and any(c.isalpha() for c in el.get(attr)): keys.add(el.get(attr))
print(f'Gebruikte i18n keys: {len(keys)}')
for code,name in langs.items():
    p=LANG/f'{code}.json'
    if not p.exists():
        print(f'❌ {code} {name}: bestand ontbreekt'); continue
    d=json.loads(p.read_text(encoding='utf-8'))
    missing=sorted(k for k in keys if k not in d and k not in d.get('_phrases',{}))
    same=[]
    if code!='nl':
        for k in keys:
            v=d.get(k,d.get('_phrases',{}).get(k))
            bv=base.get(k,base.get('_phrases',{}).get(k,k))
            if v is not None and v==bv and any(c.isalpha() for c in str(v)):
                same.append(k)
    print(f'{"✅" if not missing else "⚠️"} {code:2} {name}: missing={len(missing)}, gelijk-aan-NL={len(same)}')
    for k in missing[:15]: print('   ontbreekt:',k)
