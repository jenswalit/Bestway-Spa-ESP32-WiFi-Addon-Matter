# Building the firmware — for developers

**English** | [Nederlands](BUILD_NL.md)

Already have a programmed module? Follow the [English user manual](PtW_Spa_Control_User_Manual_EN.pdf). You do not need to build firmware to use it.

## Required build environment

| Component | Recorded baseline |
|---|---|
| Chip | ESP32-C6, 8 MB flash |
| ESP-IDF | 5.5.5 according to the supplied sdkconfig header and dependencies.lock |
| ESP-Matter | ff9f07ecb0bf01d7a2db5428149ef16bf601774c |
| connectedhomeip | 539342f32d5f4dc93761c2f9325afe29270068f1 |
| Project version | 5.0.0 |

Install ESP-IDF and ESP-Matter using the versions above. These SDKs are outside this repository. Check your installation:

```bash
idf.py --version
git -C ~/esp/esp-matter rev-parse HEAD
git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip rev-parse HEAD
```

The supplied sdkconfig also retains `CONFIG_IDF_INIT_VERSION="5.5.4"`, recording its original configuration version. Use the actual SDK version and dependency lock when checking compatibility; a directory named `release-v5.5` alone does not identify the installed patch version.

The examples below assume ESP-IDF at `~/.espressif/release-v5.5/esp-idf` and ESP-Matter at `~/esp/esp-matter`. Adjust the paths if yours differ. Managed dependencies are downloaded during the first build according to `dependencies.lock`, so network access is required.

## Download the project

Download and extract the source archive, or clone the repository:

```bash
mkdir -p ~/projects
git clone https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter.git ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
```

## Build

Open a terminal with your ESP-IDF/Matter environment active. If needed:

```bash
source ~/.espressif/release-v5.5/esp-idf/export.sh
source ~/esp/esp-matter/export.sh
cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter/firmware
idf.py build
```

The project is in `firmware/`. The supplied `sdkconfig` contains its settings. Do not delete it as a routine build step or run `set-target` unnecessarily. Check SDK version differences before changing the project configuration.

## First installation over USB

For a blank ESP32-C6, connect it using a USB data cable. From the `firmware/` directory, with the SDK environment active, use the correct serial port:

```bash
idf.py -p /dev/ttyUSB0 flash
```

`/dev/ttyUSB0` is an example for Linux; your port may differ. USB flashing is for initial installation or deliberate recovery. No erase-flash or NVS erase step is required here.

## OTA updates

After a successful build, the application file is:

`firmware/build/Bestway_Matter_v5.bin`

Upload it using the module's existing OTA page. A source-code ZIP, bootloader or partition-table file is not an application image for that page. Keep power and WiFi available throughout the update. Factory reset, erase-flash and NVS erasure are not part of a normal update.

## Checks when changing firmware

- The firmware builds successfully and fits the existing application partition.
- The HTTP server keeps 46 ordinary handlers plus 1 WebSocket within its limit of 48.
- Logging is normally off; do not add permanent log buffers or CIO diagnostic code.
- WebSocket control, actual pump target, MQTT, Matter, OTA and planner remain functional.
- Disabling a planner day affects only that day, with correct status after reloading.
- MQTT planner status and Home Assistant agree.
- Compare heap measurements under equivalent conditions; Matter commissioning/BLE affects them.

## Focused host tests

From the repository root:

```bash
bash tools/verify.sh
```

Requirements: Python 3, Node.js and g++ with C++17 support. The tests use the actual planner implementation with NVS/ESP stubs. They do not replace a full ESP-IDF build or a test on the spa.

This guide assumes an installed ESP-IDF/Matter build environment. It is not a complete SDK installation tutorial.
