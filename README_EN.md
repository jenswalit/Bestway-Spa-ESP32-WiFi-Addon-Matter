# Bestway Spa ESP32 WiFi Add-on with Matter

[ Nederlands ](README.md) | **English**

<img src="docs/assets/ptw-logo.png" alt="PtW Spa Control" width="160">

Control your Bestway Lay-Z-Spa from your phone, Home Assistant, Google Home or
Apple Home. This ESP32-C6 module sits between the pump and the original display.
The physical display remains usable.

I made this project so I can control my spa from my phone, Home Assistant or Matter. My setup is an ESP32-C6-DevKitC-1-N8 with a MIAMI2021 (6-wire Type1).

This is my hobby project, based on visualapproach's work. It is not an official Bestway WiFi module.

## Choose your starting point

| What do you have? | What should you do first? |
|---|---|
| A module with the firmware already installed | Connect the module and follow the user manual. You do not need to build the firmware yourself. |
| A working module you want to update | Download the ready-made OTA firmware from the release and upload it through the web interface. |
| A blank ESP32-C6 | Follow the build instructions and flash it over USB for the first installation. |
| You want to build the hardware yourself | Read the hardware information and the V2M hardware package. Check its verification status before ordering; the complete assembly parts list is still being finalized. |

## Download the firmware

Ready-made firmware **v5.0.1** is available in the [release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/tag/v5.0.1). Download **Bestway_Matter_v5.bin** and select it on your module's OTA page. The web interface is included, so you do not need to install it separately or build the firmware yourself for an OTA update.

The GitHub build passed and the download checksum has been checked. I build locally with Python 3.14.4 and am running v5.0.1 on my module. This version also fixes the temperature controls in demo mode.

A complete USB flashing download for a blank ESP is not available yet. Use the [build instructions](docs/BUILD_EN.md) for the first installation. The source ZIP is for building the firmware yourself.

The [build guide](docs/BUILD_EN.md#selecting-python-314) now also covers the local
Python 3.14.4 environment and updated `spa` alias. The automated GitHub build
still uses Python 3.12; using a programmed module does not require Python.

Once your module is running, start with the
**[user manual (PDF, English)](docs/PtW_Spa_Control_User_Manual_EN.pdf)**.
A [Dutch version](docs/PtW_Spa_Control_Handleiding.pdf) is also available. It explains:

1. Connecting the module to a compatible pump.
2. Waiting about 45 seconds for **LazySpaSetup** on a new module, then connecting and setting up your home WiFi.
3. Opening the web interface and controlling the spa.
4. Pairing with Matter and adding a second ecosystem.
5. Optionally setting up Home Assistant, the spa dashboard and remote access.

Home Assistant is optional. The web interface and direct Matter control also
work without Home Assistant.

## Features

- Control spa power, filtration, heating, bubbles and target temperature.
- Control HydroJets when supported by your spa model.
- Create and view daily schedules.
- View temperature, activity, history and estimated energy use.
- Update the firmware through the web interface using OTA.
- Connect multiple Matter ecosystems without removing existing pairings.
- Use a Home Assistant dashboard with three layouts and seven languages.

## Guides

The user manual and the guides below are available in English and Dutch. Each guide links to its other language.

| What would you like to do? | Read this |
|---|---|
| Install the module and learn how to use its pages | [English](docs/PtW_Spa_Control_User_Manual_EN.pdf) / [Dutch](docs/PtW_Spa_Control_Handleiding.pdf) |
| Check the connections | [Hardware and wiring](docs/HARDWARE_EN.md) |
| Make a first or additional Matter connection | [Matter pairing](docs/MATTER_EN.md) |
| Connect Spa Control through MQTT | [MQTT setup](docs/MQTT_EN.md) |
| Install the Home Assistant dashboard | [Home Assistant](home-assistant/README_EN.md) |
| Build firmware from source | [Developer build instructions](docs/BUILD_EN.md) |

Before installation, check that your pump, cables and module are compatible.
My setup is a **MIAMI2021 with 6-wire Type1** and an
**ESP32-C6-DevKitC-1-N8**. The wiring photos are not a universal wiring guide
for all Bestway models.

## Source code

`firmware/` contains the firmware and web interface. `home-assistant/` contains
the dashboard, scripts and packages. `docs/` contains the guides. Tests and
contributor information are in `tests/`, `tools/` and
[CONTRIBUTING.md](CONTRIBUTING_EN.md).

See also the [changelog](CHANGELOG_EN.md).

## Acknowledgements and license

This project is based on
[WiFi-remote-for-Bestway-Lay-Z-SPA by visualapproach](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA).
Thanks to visualapproach and the contributors for their original work.

I adapted this version for the ESP32-C6 with ESP-IDF, ESP-Matter and
the accompanying Home Assistant dashboard. It is a separate project;
its changes and support are not provided by the original project.

The project software is released under the **GNU General Public License
version 3 (GPL-3.0)**. See [LICENSE](LICENSE) for the full text.
Third-party components retain their own licenses. See also the
[origin and licensing information](docs/ORIGIN_AND_LICENSES_EN.md).
