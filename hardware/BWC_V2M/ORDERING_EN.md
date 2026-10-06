# Ordering the BWC V2M bare PCB

[Nederlands](BESTELLEN.md) · [Hardware overview](README_EN.md)

## Ordering status

**PCB source and Gerber checked for track segments and drills; DRC and physical board test not yet documented.**

The 6 October 2026 export includes two copper layers, solder masks, silkscreens, board outline and drill files. The outline measures 98.1964 × 64.6684 mm, with four 4.064 mm mounting holes. The five vias belong to the latest PCB source included here.

All 220 straight track segments, 140 component drills, 5 via drills and 4 mounting holes were compared with the Gerber. The 140 pin/net connections match the schematic. Full copper-pour geometry and connectivity have not been checked.

Run DRC in EasyEDA and inspect the Gerber preview before ordering. A design fault remains possible until the board has been physically tested.

## What you receive

A **bare circuit board**. Purchase and fit the ESP32-C6, level shifter, resistors, connectors and other parts separately. This Gerber archive is neither firmware nor a complete automated assembly package. A final parts list and assembly guide will follow separately.

## Ordering steps

1. Open the GitHub hardware page and check ordering status and board revision.
2. Download only [BWC_SPA_CONTROL_V2M_2026-10-06.zip](gerber/BWC_SPA_CONTROL_V2M_2026-10-06.zip) using the file's download button. **Do not unzip it.** Do not upload the full repository ZIP or the complete hardware package.
3. Open a manufacturer such as [JLCPCB](https://jlcpcb.com/) and select **Order Now / Instant Quote**. Create an account if needed.
4. Select **Add your Gerber file** and upload the Gerber ZIP. The manufacturer analyses layers and dimensions.
5. Check **2 copper layers** and approximately **98.20 × 64.67 mm**. Select one design, individual boards and your desired quantity.
6. Use **Gerber Viewer** to inspect both sides, the outline, mounting holes and two ESP32 header rows. Return to the design if holes, layers or the outline are unexpected.
7. Confirm board thickness, copper weight and surface finish. These manufacturing choices have not yet been confirmed; Gerbers alone do not fully specify these choices. Disable PCB Assembly for a bare-board order.
8. Check quantity, shipping address and total cost, including shipping and applicable taxes. Place the order yourself. A manufacturer's file review does not replace electrical design verification.

Official instructions: [JLCPCB — How do I place a PCB order?](https://jlcpcb.com/help/article/how-do-i-place-an-order).

## Sharing the link

Share the [project page](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter) or, once these files have been uploaded, the [hardware directory](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/tree/main/hardware/BWC_V2M). Visitors can download files from a public repository without a GitHub account.

An EasyEDA editor link only works for others when they have access to that project. GitHub with the Gerber archive and these instructions is the simplest starting point for people ordering a board.
