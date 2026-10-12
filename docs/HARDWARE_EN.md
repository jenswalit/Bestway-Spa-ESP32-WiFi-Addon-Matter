# Hardware and wiring

**English** | [Nederlands](HARDWARE_NL.md)

The working setup uses an **ESP32-C6-DevKitC-1-N8**, **MIAMI2021** and **six-wire Type1** communication. Check the connections for your own spa model and the status of your chosen PCB revision before installing it.

| Connection | Signal | GPIO |
|---|---|---:|
| CIO — pump side | DATA | 23 |
| CIO — pump side | CLK | 22 |
| CIO — pump side | CS | 21 |
| DSP — physical display | DATA | 20 |
| DSP — physical display | CLK | 19 |
| DSP — physical display | CS | 18 |
| Audio | LEDC output | 11 |
| Reserved temperature connection | Reserved | 0 |
| RGB status LED | LED | 8 |

## Installing the module

1. Turn the spa off and unplug its mains plug.
2. Disconnect the original display plug from the pump circuit board.
3. Connect that display plug to the module's DSP side.
4. Connect the module's CIO side to the now-empty connector on the pump board.
5. Mount the module in a dry, protected position and check cables and closure.
6. Close the pump housing, restore power and configure WiFi.

The module goes **between the original pump and display connectors**. Leave mains wiring, protective earth connections and power cables untouched.

See the installation section and photos in the [English user manual](PtW_Spa_Control_User_Manual_EN.pdf). The photos describe the documented setup rather than a universal connector arrangement for all spa models.

## PCB sources and Gerber files

The V2M hardware package includes a schematic, editable EasyEDA sources, an ESP32-C6 footprint and a Gerber ZIP. See the [English V2M hardware overview](../hardware/BWC_V2M/README_EN.md) and [ordering guide](../hardware/BWC_V2M/ORDERING_EN.md).

The latest checked V2M export has five vias. Its straight track segments and drills were compared with the supplied PCB source, and 140 pin/net assignments match the schematic. Full copper-pour connectivity, DRC and physical prototype validation are separate checks. Consult that revision's documented verification status before ordering.

A bare PCB order excludes the ESP32-C6, level shifter, connectors and other components. The complete assembly BOM and assembly instructions remain to be finalized.
