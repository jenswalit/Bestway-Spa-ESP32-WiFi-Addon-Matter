# Building the firmware — for developers

**English** | [Nederlands](BUILD_NL.md)

Just updating your module? Download the ready-made `Bestway_Matter_v5.bin` from the [latest release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/latest) and use the OTA page. You do not need to build it yourself.

Already have a programmed module? Follow the [English user manual](PtW_Spa_Control_User_Manual_EN.pdf). You do not need to build firmware to use it.

## Required build environment

| Component | Recorded baseline |
|---|---|
| Chip | ESP32-C6, 8 MB flash |
| ESP-IDF | 5.5.5 according to the supplied sdkconfig header and dependencies.lock |
| ESP-Matter | ff9f07ecb0bf01d7a2db5428149ef16bf601774c |
| connectedhomeip | 539342f32d5f4dc93761c2f9325afe29270068f1 |
| Local Python | 3.14.4, separate ESP-IDF environment |
| GitHub Actions Python | 3.12, existing CI baseline |
| Project version | 5.0.1 |

Install ESP-IDF and ESP-Matter using the versions above. These SDKs are outside this repository. Check your installation:

```bash
idf.py --version
git -C ~/esp/esp-matter rev-parse HEAD
git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip rev-parse HEAD
```

The supplied sdkconfig also retains `CONFIG_IDF_INIT_VERSION="5.5.4"`, recording its original configuration version. Use the actual SDK version and dependency lock when checking compatibility; a directory named `release-v5.5` alone does not identify the installed patch version.

The examples below assume ESP-IDF at `~/.espressif/release-v5.5/esp-idf` and ESP-Matter at `~/esp/esp-matter`. Adjust the paths if yours differ. Managed dependencies are downloaded during the first build according to `dependencies.lock`, so network access is required.

## Selecting Python 3.14

Python runs on the development computer, not on the ESP32. Changing its version
does not require changing Matter fabrics or device settings. The local build
with Python 3.14.4 works. The firmware has also been built on GitHub with Python
3.12, completing the full build successfully. You do not need Python for OTA.

ESP-IDF uses a separate environment for each Python version. If
`idf5.5_py3.14_env` is missing, create it using the installed Python 3.14:

```bash
(
  export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env"
  /usr/bin/python3 ~/.espressif/release-v5.5/esp-idf/tools/idf_tools.py install-python-env
)
```

First check that `/usr/bin/python3 --version` actually reports 3.14. This command
installs IDF Python dependencies; any additional ESP-Matter dependencies remain
part of ESP-Matter setup. Keep the existing 3.12 environment as a fallback.

Use the explicit `IDF_PYTHON_ENV_PATH` from the build example in your `spa` alias
as well. For a clone using this repository's directory layout, put this in
`~/.bashrc`:

```bash
alias spa='export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env" && source ~/.espressif/release-v5.5/esp-idf/export.sh && source ~/esp/esp-matter/export.sh && cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter/firmware'
```

Adjust the project path if your firmware is directly in
`~/projects/Bestway_Matter_v5`. Reload with `source ~/.bashrc`, run `spa`, then
check `python --version` and `idf.py --version`.

If the build directory still refers to Python 3.12, run `idf.py fullclean`, then
`idf.py build` from the firmware directory. This does not erase source files,
`sdkconfig` or device NVS.

If cleanup reports a modified or incomplete managed component, preserve its
contents outside `managed_components`, keep `dependencies.lock`, and run
`idf.py reconfigure` followed by `idf.py build` to fetch the locked dependency
again.

## Download the project

Download and extract the source archive, or clone the repository:

```bash
mkdir -p ~/projects
git clone https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter.git ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
```

## Build

Open a terminal with your ESP-IDF/Matter environment active. If needed:

```bash
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.14_env"
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

After switching on a new module, wait about 45 seconds for the **LazySpaSetup**
WiFi network to appear, then follow the user manual.

## OTA updates

After a successful build, the application file is:

`firmware/build/Bestway_Matter_v5.bin`

Upload it using the module's existing OTA page. A source-code ZIP, bootloader or partition-table file is not an application image for that page. Keep power and WiFi available throughout the update. Factory reset, erase-flash and NVS erasure are not part of a normal update.

## Version every firmware update

For every firmware update, increment the project version in
`firmware/CMakeLists.txt`, the monotonically increasing Matter
`PROJECT_VER_NUMBER`, and the web interface `app-version`. The Matter version
string and number are derived from these CMake values. Update the changelog as
well. Documentation-only changes do not increment the firmware version.

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

