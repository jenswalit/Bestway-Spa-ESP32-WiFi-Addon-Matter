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
a=s.index('struct LiveCommandResult {')
b=s.index('static esp_err_t api_command(',a)
Path('tests/out/demo_command.inc').write_text(s[a:b])
a=s.index('static std::string status_json()')
b=s.index('static std::string json_escape(',a)
Path('tests/out/demo_status.inc').write_text(s[a:b])
bus=Path('firmware/components/bestway_bus/bestway_bus.cpp').read_text()
# Compile the actual simulation branches; hardware protocol drivers are stubbed.
parts=[]
for name in ['set_target_temperature', 'step_target']:
    a=bus.index('void BestwayBus::'+name+'(')
    b=bus.index('    if (model_info().family ==',a)
    parts.append(bus[a:b]+'    ++hardware_steps;\n}\n')
Path('tests/out/demo_bus.inc').write_text('\n'.join(parts))
begin=bus[bus.index('void BestwayBus::begin()'):bus.index('void BestwayBus::loop()')]
assert 'set_target_temperature(39);' in begin
assert 'spa.set_target_temperature(39);' not in begin

PY
for test in scheduler handler; do
  g++ -std=c++17 -Wall -Wextra -Wno-unused-variable -Wno-missing-field-initializers \
    -Itests/stubs -Itests/out -Ifirmware/components/scheduler/include \
    "tests/test_${test}.cpp" firmware/components/scheduler/scheduler_manager.cpp \
    -o "tests/out/test_${test}"
  "tests/out/test_${test}"
done
g++ -std=c++17 -Wall -Wextra -Itests/stubs -Itests/out \
  -Ifirmware/components/spa_core/include tests/test_demo.cpp \
  firmware/components/spa_core/spa_state.cpp -o tests/out/test_demo
tests/out/test_demo
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
cmake=Path('firmware/CMakeLists.txt').read_text()
version=re.search(r'set\(PROJECT_VER "([^"\n]+)"\)',cmake).group(1)
html=Path('firmware/components/web_server/web/index.html').read_text()
assert f'content="{version}" name="app-version"' in html
assert 'DEVICE_SOFTWARE_VERSION=${PROJECT_VER_NUMBER}' in cmake
assert r'DEVICE_SOFTWARE_VERSION_STRING=\"${PROJECT_VER}\"' in cmake
print(f'PASS: firmware/web version {version}; Matter version derived from project settings')
PY

