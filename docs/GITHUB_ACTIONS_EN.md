# Building firmware on GitHub

**English** | [Nederlands](GITHUB_ACTIONS_NL.md)

I also build the firmware on GitHub, so you can download a ready-made OTA image without installing ESP-IDF and ESP-Matter yourself.

## Update your module

The full **v5.0.1** firmware build passed: [build #6](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/actions/runs/38127239675).
The ready-made **Bestway_Matter_v5.bin** is available in the [release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/tag/v5.0.1), with its checksum and build details.

1. Download the .bin file under **Assets**.
2. Open your module's OTA page.
3. Select the file and start the update. Keep power and WiFi available.
4. Wait for the restart and check the version.

The web interface is included in the firmware. Normal OTA updates retain your settings and Matter pairings; no factory reset or NVS erasure is needed.

A blank ESP32-C6 needs its first installation over USB. The release does not include a complete USB installation package yet; follow the [build instructions](BUILD_EN.md).

## My build environment

- GitHub runner: Ubuntu 22.04 with Python 3.12.
- ESP-IDF: v5.5.5.
- ESP-Matter: `ff9f07ecb0bf01d7a2db5428149ef16bf601774c`.
- connectedhomeip: `539342f32d5f4dc93761c2f9325afe29270068f1`.
- Target: ESP32-C6 with the supplied `firmware/sdkconfig` and partitions.

On my own computer I build with Python 3.14.4. Both builds pass. The workflow checks SDK revisions and runs the project checks before compiling the firmware.

## Start your own GitHub build

The workflow is already in this repository. You may need to enable Actions in your own fork first.

1. Open **Actions**.
2. Select **Build ESP32-C6 firmware**.
3. Select **Run workflow → main → Run workflow**.
4. Open the run to follow its progress.

Changes to firmware, tests, `tools/verify.sh` or the build workflow also trigger a build automatically. Documentation-only and hardware changes do not.

Downloading SDKs and compiling can take a while. The workflow has a 90-minute limit. If it fails, open the failed step to read its log.

## Download your own build

After a successful run, download `Bestway_ESP32C6_OTA_<run number>` under **Artifacts**. Extract the ZIP and use the .bin file on the OTA page. Do not upload the ZIP itself to the module.

The download contains:

- `Bestway_Matter_v5.bin`: firmware with the web interface included.
- `SHA256SUMS.txt`: checksum.
- `build-info.json`: source commit, SDK versions and file details.
- `README.txt`: brief OTA instructions.

Artifacts are normally kept for 30 days and generally require a GitHub login. Published firmware is also available under [Releases](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases).

## How I publish releases

The build workflow compiles and stores the OTA image. A separate publication workflow checks the successful build, source commit and checksum, then attaches that image to the release. Neither workflow updates your module automatically; you start OTA yourself through the web interface.
