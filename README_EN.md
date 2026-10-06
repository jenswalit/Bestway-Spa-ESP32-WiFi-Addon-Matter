# Bestway Spa ESP32 WiFi Add-on with Matter

[ Nederlands ](README.md) | **English**

<img src="docs/assets/ptw-logo.png" alt="PtW Spa Control" width="160">

Control your Bestway Lay-Z-Spa from your phone, Home Assistant, Google Home or
Apple Home. This ESP32-C6 module sits between the pump and the original display.
The physical display remains usable.

This is an independent hobby project, not an official Bestway WiFi module.

## Choose your starting point

| What do you have? | What should you do first? |
|---|---|
| A module with the firmware already installed | Connect the module and follow the user manual. You do not need to build the firmware yourself. |
| A blank module and compatible prebuilt firmware | Flash the firmware over USB first. You do not need to build it yourself if a suitable firmware file is available. |
| A blank module and only the source code | Install the build environment, build the firmware and flash the module over USB. |
| You want to build the hardware yourself | Read the hardware information. Schematics, Gerber files and a complete parts list will be added later. |

This repository contains source code. This does not automatically mean that
prebuilt firmware is available. A GitHub source ZIP cannot be flashed directly
onto an ESP. See the [build instructions](docs/BUILD_NL.md) to build from source.

Once your module is running, start with the
**[user manual (PDF, English)](docs/PtW_Spa_Control_User_Manual_EN.pdf)**.
A [Dutch version](docs/PtW_Spa_Control_Handleiding.pdf) is also available. It explains:

1. Connecting the module to a compatible pump.
2. Connecting to the module's WiFi access point and configuring your home WiFi.
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

The user manual is available in English and Dutch. The other linked guides are currently in Dutch.

| What would you like to do? | Read this |
|---|---|
| Install the module and learn how to use its pages | [English](docs/PtW_Spa_Control_User_Manual_EN.pdf) / [Dutch](docs/PtW_Spa_Control_Handleiding.pdf) |
| Check the connections | [Hardware and wiring](docs/HARDWARE_NL.md) |
| Make a first or additional Matter connection | [Matter pairing](docs/MATTER_NL.md) |
| Install the Home Assistant dashboard | [Home Assistant](home-assistant/README.md) |
| Build firmware from source | [Developer build instructions](docs/BUILD_NL.md) |

Before installation, check that your pump, cables and module are compatible.
The documented test setup is a **MIAMI2021 with 6-wire Type1** and an
**ESP32-C6-DevKitC-1-N8**. The wiring photos are not a universal wiring guide
for all Bestway models.

## Source code

`firmware/` contains the firmware and web interface. `home-assistant/` contains
the dashboard, scripts and packages. `docs/` contains the guides. Tests and
contributor information are in `tests/`, `tools/` and
[CONTRIBUTING.md](CONTRIBUTING.md).

See also the [changelog](CHANGELOG.md).

## Acknowledgements and license

This project is based on
[WiFi-remote-for-Bestway-Lay-Z-SPA by visualapproach](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA).
Thanks to visualapproach and the contributors for their original work.

This modified version targets the ESP32-C6 with ESP-IDF, ESP-Matter and
the accompanying Home Assistant dashboard. It is a separate project;
its changes and support are not provided by the original project.

The project software is released under the **GNU General Public License
version 3 (GPL-3.0)**. See [LICENSE](LICENSE) for the full text.
Third-party components retain their own licenses. See also the
[licensing information (Dutch)](docs/HERKOMST_EN_LICENTIES.md).
