# ESP32-C6 firmware

**English** | [Nederlands](README.md)

This is the working source code with the planner per-day fix confirmed on 6 October 2026.

Build from this directory using your installed ESP-IDF and ESP-Matter environment:

```bash
idf.py build
```

The output is `build/Bestway_Matter_v5.bin`. Use the existing OTA page for normal updates. See [BUILD_EN.md](../docs/BUILD_EN.md) for full steps and SDK versions.

`components/web_server/web/` contains the embedded web assets. The supplied `sdkconfig`, `sdkconfig.defaults`, partition table and dependency lock belong to the working baseline. The GitHub directory structure does not change the firmware sources.
