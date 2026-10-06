# Bill of materials — BWC V2M / ESP32-C6

**English** | [Nederlands](BOM.md) · [Hardware overview](README_EN.md)

This **design BOM** is derived from `PCB_BWC_SPA_CONTROL_V2M_2026-10-06.json`, the five-via revision. Values and quantities come from the design data. It supports manual construction; it is not yet a fully confirmed shopping list or an automated assembly BOM.

Downloads: [grouped BOM.csv](BOM.csv) · [all 27 component positions and source metadata](BOM_Source.csv).

## Board and passive components

| Reference | Design quantity | Part / value | Mounting |
|---|---:|---|---|
| PCB | 1 | BWC SPA CONTROL V2M, 2 layers | Bare board; see [ordering guide](ORDERING_EN.md) |
| U1 | 1 | ESP32-C6-DevKitC-1-N8 | Complete DevKit; 2 × 16 pins, 2.54 mm pitch, 22.86 mm row spacing |
| R2–R8 | 7 | 560 Ohm | Axial through-hole; pad spacing 5.25 mm |
| R10 | 1 | 4.7 kOhm | Axial through-hole; pad spacing 7.30 mm; GPIO0 pull-up to 3V3 |
| C2–C8 | 7 | 220 pF | Ceramic through-hole; lead pitch 5.00 mm |

There are **no R9 or C1** in this design. Resistor ratings, tolerances, capacitor selection and mechanical fit require confirmation when choosing final parts. The CSV records original library part numbers without presenting them as guaranteed suitable replacements.

## Level shifter — select one variant

| Reference | Quantity | Variant | Footprint |
|---|---:|---|---|
| U2 | 1 if selected | 8-channel bidirectional MOSFET level-shifter module, 3.3 ↔ 5 V | 2 × 10 pins; 2.54 mm pitch; 11.43 mm row spacing |
| U3 | 1 if selected | TXS0108E level-shifter module | 2 × 10 pins; 2.54 mm pitch; 12.70 mm row spacing |
| R1 | 1, only with U3 | 10 kOhm axial resistor | Pad spacing 5.25 mm; design text: “10K only with U3” |

**Fit U2 or U3, not both.** Compare the complete pinout and dimensions with the schematic; the description “8-channel level shifter” does not establish compatibility.

The U3 library names `TXS0108EPWR` / `C17206`. This identifies the **bare TSSOP chip**, not the complete module with header pins expected by this PCB. Do not use it as the module's order code. Exact order codes for both module variants remain unconfirmed.

R1 is connected between GND and 3V3 in the PCB, so this BOM does not describe it as an OE pull-down. The U3 OE pin is connected to 3V3 in the design.

## U2: generic module type, no mandatory brand

The intended U2 type is an **8-channel bidirectional 3.3 V ↔ 5 V logic-level-converter module with individual MOSFET channels**, as in the supplied example. No specific brand, PCB colour or seller is required. Suggested search description: `8 channel bidirectional MOSFET logic level converter 3.3V 5V`.

The exact transistor code of the example has not been confirmed. Before purchasing, check that the module has this mechanical arrangement and electrical pin order:

- Two rows of ten pins, 2.54 mm pitch and 11.43 mm row spacing.
- Low side at 3V3 (LV), high side at 5V (HV), common GND.
- Channel and supply pins must match the footprint order below. Another module with the same pin count is not automatically compatible.

| Footprint position | Low side J1 | High side J2 |
|---|---|---|
| 1–4 | LV channels 1–4 | HV channels 1–4 |
| 5 | LV / 3V3 | HV / 5V |
| 6 | GND | GND |
| 7–10 | LV channels 5–8 | HV channels 5–8 |

Use the numbering in the schematic rather than assuming a top-to-bottom order from a shop image. U3 remains a separate alternative TXS0108E module position; do not fit it together with U2.

## Connectors and auxiliary headers

| Reference | Design positions | Function | Geometry |
|---|---:|---|---|
| H1A, H1B | 2 | CIO / pump side, A and B connections | 1 × 6 pins, 2.54 mm pitch |
| H2A, H2B | 2 | DSP / display side, A and B connections | 1 × 6 pins, 2.54 mm pitch |
| J1 | 1 | GND extension | 1 × 4 pins, 2.54 mm pitch |
| J3 | 1 | 3V3 extension | 1 × 4 pins, 2.54 mm pitch |
| J2 | 1 | GPIO0 / reserved temperature connection | 1 × 2 pins, 2.54 mm pitch |
| J4 | 1 | GPIO10 | 1 × 2 pins, 2.54 mm pitch |

The four H positions are **design quantities**. Actual connector population depends on the spa cable assembly. Confirm the connector type, mating plug, orientation and population. The source lists Molex `22284060` / `C587040`; that does not establish that it directly fits your spa plug.

J headers are optional extensions. “Jumper2” in the library is not an instruction to short their pins.

## Additional assembly items

These items have no separate PCB reference and are not included in the 27 design components:

- Two 16-pin female headers for a removable ESP32-C6, if appropriate for your DevKit version and mounting height.
- Suitable headers or sockets for the chosen level-shifter module, if not included with it.
- Pump/display cables with suitable connectors and verified pin assignments.
- Enclosure, fixing hardware, spacers and strain relief suitable for the installation.

No separate RGB LED or buzzer appears in the PCB BOM. GPIO8 uses the DevKit LED; a separate audio component is not specified in these design files.

## Checks and references

All 27 component positions are included. Grouped resistors and capacitors were checked for matching source values. CSV files contain no price or stock claims. Complete assembly and physical testing of this V2M board have not yet been documented.

Source identity: [BOM_SOURCE.txt](BOM_SOURCE.txt). Product checks: [TI TXS0108EPWR — bare TSSOP-20 chip](https://www.ti.com/product/TXS0108E/part-details/TXS0108EPWR), [LCSC C249163 — 220 pF capacitor](https://www.lcsc.com/product-detail/C249163.html).
