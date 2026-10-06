# BWC PCB V2M — ESP32-C6

[Nederlands](README.md)

Addon board for an ESP32-C6-DevKitC-1-N8, intended for the firmware in this repository. The module connects between the spa pump and its physical display.

This is an independent hobby project, not an official Bestway Wi-Fi module.

## Available files

| File | Purpose |
| --- | --- |
| [PCB_BWC_V2M.json](easyeda/PCB_BWC_V2M.json) | Editable PCB for EasyEDA Standard |
| [SCH_BWC_V2M.json](easyeda/SCH_BWC_V2M.json) | Matching electrical schematic |
| [PCBLIB_BWC_ESP32C6_V2M.json](easyeda/PCBLIB_BWC_ESP32C6_V2M.json) | ESP32-C6 footprint with 32 header pads |
| [Schema.pdf](Schema.pdf) | Readable electrical overview |

## Ordering the board

The [6 October 2026 Gerber ZIP](gerber/BWC_SPA_CONTROL_V2M_2026-10-06.zip) and its matching latest PCB source are included. See the [English ordering guide](ORDERING_EN.md).

**Status: track segments and drill files compared with PCB source; DRC and physical board test not yet documented.** Full copper-pour geometry and connectivity have not been checked. Run DRC and inspect the Gerber preview before ordering.

A bare-board order excludes the ESP32, level shifter and other parts. A complete assembly BOM and assembly guide have not yet been published.

[EasyEDA project](https://easyeda.com/editor#project_id=5c570d72ac544b518ed51a159b791d9f) — access for other users depends on project visibility.

## Opening the design in EasyEDA Standard

1. Download the JSON using GitHub's Raw/download button, or download the repository ZIP.
2. In EasyEDA Standard select **File → Open → EasyEDA** and open the file.
3. Keep one matching schematic and PCB revision in the same project. Import the footprint library separately if needed.
4. U1 is linked to the ESP32-C6 footprint in this PCB. The previous standalone header pads have been replaced without moving their positions.

## Connections used by the current firmware

| Function | ESP32-C6 GPIO |
| --- | --- |
| CIO DATA — pump side | 23 |
| CIO CLK — pump side | 22 |
| CIO CS — pump side | 21 |
| DSP DATA — display side | 20 |
| DSP CLK — display side | 19 |
| DSP CS — display side | 18 |
| AUDIO | 11 |
| Reserved temperature connection | 0 |
| DevKit onboard RGB status LED | 8 |

GPIO10 is also brought out to an additional connector. It is not one of the spa bus GPIOs listed above.

U2 and U3 are alternative level-shifter modules in this design. The final assembly BOM must specify which version is fitted. R10 is a 4.7 kOhm pull-up to 3V3.

## Checks performed

Checks cover the supplied `PCB_BWC_SPA_CONTROL_V2M_2026-10-06.json` and Gerber export dated 6 October 2026:

- All 140 component pin/net connections and 27 component associations match the schematic.
- CIO, DSP, audio and reserved GPIO assignments match.
- All 220 straight segments in the 115 copper tracks were found in their corresponding Gerber copper layers, including widths.
- All 140 component drills and 5 via drills match locations and diameters within a 0.002 mm comparison tolerance.
- The 4 mounting holes match; diameter 4.064 mm.
- ZIP integrity checks pass. The supplied Gerber archive is included unchanged.

The five vias belong to the latest design. The previous seven-via source has been replaced, resolving that discrepancy.

See [CONTROLE.txt](gerber/CONTROLE.txt) for technical checks. This comparison does not constitute a complete copper-pour, electrical copper-connectivity or clearance check. EasyEDA DRC and a physical manufactured V2M board test have not yet been documented.

## Origin and licensing

Based on [visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA).

See this repository's [LICENSE](../../LICENSE) and [origin information](../../docs/HERKOMST_EN_LICENTIES.md) for software licensing and attribution. A separate hardware licence has not yet been established for this package; the software licence is not automatically extended to PCB design files here.
