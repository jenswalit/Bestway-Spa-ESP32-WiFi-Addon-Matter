# Changelog

**English** | [Nederlands](CHANGELOG.md)

## Documentation — 2026-10-11

- Updated both manuals: LazySpaSetup appears about 45 seconds after switching on a new module.
- Rewrote the GitHub guides and release notes in my own voice, with direct links to the ready-made OTA firmware.
- Removed outdated build status and references to hardware files still awaiting upload. Firmware remains at 5.0.1.

## 5.0.1 — 2026-10-10

- Firmware and web version 5.0.1; Matter software version increased from 5001 to 5002. Every firmware update receives a new version.
- The simulated target temperature is immediately valid and visible; +/- works with a selected 4-wire model without Godmode.
- Hardware restrictions for temperature, heating and Power apply only in hardware mode (web, MQTT and Matter).
- Build instructions describe the local Python 3.14.4 environment, the `spa` alias and reconfiguration after switching Python. GitHub Actions retains Python 3.12.

## 2026-10-06

- Added English build, Matter, hardware, MQTT, Home Assistant, contributor and licensing guides with links between languages.
- Added an English user manual with a table of contents, screenshots and installation photos.
- Documentation focuses on installation and use by new users.
- Web planner: disabling one day leaves the remaining days enabled.
- A disabled day remains visible as a separate planner rule.
- Re-enabling a day merges matching rules.
- Maximum 24 planner rules; if there is insufficient space, the schedule remains unchanged.

## 2026-10-05 — User manual 0.4

- Installation photos and instructions for connecting between pump and display.
- WiFi, Matter, Home Assistant and remote-access instructions.

## Existing features

Live web control through WebSocket, MQTT, OTA, automatic unlocking, unique Matter onboarding credentials per module, multiple Matter fabrics, planner, history and a Home Assistant dashboard. Debug logging is off by default.

