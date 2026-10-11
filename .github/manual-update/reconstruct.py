import base64, hashlib, json, re, struct, zlib, zipfile
from pathlib import Path

payload = ''.join(p.read_text() for p in sorted(Path('.github/manual-update').glob('payload-*.txt')))
items = json.loads(zlib.decompress(base64.b64decode(payload)))
original_shas = {
 'docs/PtW_Spa_Control_Handleiding.docx': '6b027fa98e1186a85a274b64c5813bea0833999e',
 'docs/PtW_Spa_Control_Handleiding.pdf': '9dc7400b8d3b063ec0201115e7ab2d739f9658c6',
 'docs/PtW_Spa_Control_User_Manual_EN.docx': '26ef42ee06ec94791b080c3daf86fa869003bb85',
 'docs/PtW_Spa_Control_User_Manual_EN.pdf': '1216878639d707de2a41869aaad3ab72a09b8759',
}
outputs = {}
for item in items:
 path = Path(item['path'])
 assert item['path'] in original_shas
 old = path.read_bytes()
 if hashlib.sha256(old).hexdigest() == item['sha256']:
  print('Already updated:', path)
  continue
 blob_sha = hashlib.sha1(b'blob ' + str(len(old)).encode() + b'\0' + old).hexdigest()
 assert blob_sha == original_shas[item['path']], f'Original document changed: {path}'
 candidates = {}
 def remember(raw):
  candidates[hashlib.sha256(raw).hexdigest()] = raw
 if path.suffix == '.docx':
  with zipfile.ZipFile(path) as archive:
   for entry in archive.infolist():
    if entry.filename.startswith('word/media/'):
     n, e = struct.unpack_from('<HH', old, entry.header_offset + 26)
     a = entry.header_offset + 30 + n + e
     remember(old[a:a + entry.compress_size])
 else:
  for match in re.finditer(rb'(?<!end)stream\r?\n(.*?)\r?\nendstream', old, re.S):
   raw = match.group(1)
   for suffix in (b'', b'\n', b'\r', b'\r\n'):
    remember(raw + suffix)
 parts = []
 for part in item['parts']:
  if 'data' in part:
   parts.append(base64.b64decode(part['data']))
  else:
   assert part['sha256'] in candidates, f'Missing image data in {path}'
   raw = candidates[part['sha256']]
   assert len(raw) == part['size']
   parts.append(raw)
 updated = b''.join(parts)
 assert hashlib.sha256(updated).hexdigest() == item['sha256'], f'Output mismatch: {path}'
 if path.suffix == '.docx':
  import io
  with zipfile.ZipFile(io.BytesIO(updated)) as archive:
   assert archive.testzip() is None
   assert b'45' in archive.read('word/document.xml')
 outputs[path] = updated
 print('Verified exact reviewed document:', path, item['sha256'])
for path, updated in outputs.items():
 path.write_bytes(updated)
