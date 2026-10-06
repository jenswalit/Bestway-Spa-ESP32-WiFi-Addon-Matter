#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p tests/out
python3 - <<'PY'
from pathlib import Path
s=Path('firmware/components/web_server/web_server.cpp').read_text()
a=s.index('static esp_err_t api_schedules_put(')
b=s.index('static esp_err_t api_schedules_delete(',a)
Path('tests/out/handler.inc').write_text(s[a:b])
PY
for test in scheduler handler; do
  g++ -std=c++17 -Wall -Wextra -Wno-unused-variable -Wno-missing-field-initializers \
    -Itests/stubs -Itests/out -Ifirmware/components/scheduler/include \
    "tests/test_${test}.cpp" firmware/components/scheduler/scheduler_manager.cpp \
    -o "tests/out/test_${test}"
  "tests/out/test_${test}"
done
node --check firmware/components/web_server/web/js/app.js
node --check home-assistant/www/bestway-language-card.js
node tests/test_web.cjs
python3 - <<'PY'
from pathlib import Path
import re
s=Path('firmware/components/web_server/web_server.cpp').read_text()
n=len(re.findall(r'^\s*register_uri\(',s,re.M))+1
assert n<=48,(n,'HTTP URI limiet overschreden')
print(f'PASS: {n} URI handlers inclusief WebSocket; limiet 48')
PY
