# ESP32-C6 firmware

**English** | [Nederlands](README.md)

This is the source for the ESP32-C6 firmware **v5.0.1**, including the planner per-day fix and improved demo-mode temperature controls.

Just updating an existing module? Download the ready-made `Bestway_Matter_v5.bin` from the [latest release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/latest) and upload it on the OTA page. There is no need to build it yourself.

Build from this directory using your installed ESP-IDF and ESP-Matter environment:

```bash
idf.py build
```

The output is `build/Bestway_Matter_v5.bin`. Use the existing OTA page for normal updates. See [BUILD_EN.md](../docs/BUILD_EN.md) for full steps and SDK versions.

`components/web_server/web/` contains the embedded web assets. The supplied `sdkconfig`, `sdkconfig.defaults`, partition table and dependency lock belong to the working baseline. The GitHub directory structure does not change the firmware sources.
