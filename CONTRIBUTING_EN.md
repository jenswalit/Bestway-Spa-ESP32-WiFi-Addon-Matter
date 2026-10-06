# Contributing to this project

**English** | [Nederlands](CONTRIBUTING.md)

Make small, focused changes. Describe the problem, resulting behavior and validation in a pull request. Do not change the SDK, bus implementation and UI together without a concrete need.

Web assets are embedded in the firmware. Preserve WebSocket-first control, MQTT, Matter multi-admin, OTA, planner and Home Assistant support. Log buffers are needed only when the user manually enables debug/live logging.

Do not add HTTP handlers without checking the handler count. Do not invent a default target temperature after reboot. Do not use factory reset or NVS erasure as a general troubleshooting step. If an OTA update causes a problem, examine the report or log first.

Run `bash tools/verify.sh` for planner or web-planner changes, then build in the matching ESP-IDF/Matter environment and describe the hardware test. Do not commit personal exports, Matter setup codes, network credentials or flash dumps.
