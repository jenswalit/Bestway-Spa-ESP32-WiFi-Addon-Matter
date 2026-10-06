# Home Assistant spa dashboard

**English** | [Nederlands](README.md)

Install the spa dashboard with three layouts and seven languages. The firmware connects to Home Assistant through MQTT. Matter may remain active alongside MQTT.

## Files

| File | Destination in Home Assistant |
|---|---|
| `www/bestway-language-card.js` | `www/` next to configuration.yaml |
| `packages/bestway_opwarming_package.yaml` | `packages/` next to configuration.yaml |
| `packages/bestway_gebruik_vandaag_package.yaml` | The same packages directory |
| `layzspa_planner_popup_scripts.yaml` | Merge its contents into existing scripts.yaml |
| `layzspa_dashboard_3_layouts_7_talen.yaml` | Complete raw dashboard configuration |

On Home Assistant OS the configuration directory is usually `/config`. Do not create another `/config` directory or duplicate top-level `homeassistant:`, `packages:` or `script:` keys.

## Requirements

- Home Assistant with an MQTT integration and an MQTT broker. See [MQTT setup](../docs/MQTT_EN.md).
- HACS for additional frontend cards.
- button-card, layout-card, ApexCharts Card and card-mod.
- Browser Mod downloaded as a component **and** added as an active Home Assistant integration.
- Buienradar integration with `weather.buienradar` for outdoor temperature and heating-time estimates.

See the Home Assistant chapter in the [English user manual](../docs/PtW_Spa_Control_User_Manual_EN.pdf) for beginner instructions.

With the MQTT base topic `layzspa`, the dashboard expects entities for power, filter, heater, bubbles, water temperature, target temperature, actual heating and planner. HydroJets controls appear only when the corresponding entity exists.

## Installation

1. Copy the JavaScript file to Home Assistant's `www/` directory.
2. Add or update the dashboard resource to `/local/bestway-language-card.js?v=4`, type **JavaScript module**. Update an existing entry rather than adding a duplicate.
3. Place both packages in `packages/`. Keep your existing packages configuration. If packages are not configured yet, use this within the single `homeassistant:` section:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

4. Merge the three named planner scripts into your existing `scripts.yaml`. The downloaded script file does not require an extra `script:` wrapper. Preserve existing scripts.
5. Check the YAML configuration, reload Scripts and restart Home Assistant where required.
6. Paste the full dashboard YAML into a dashboard's raw configuration editor. The file includes both `button_card_templates` and `views`; it is not a single card fragment. Use a separate dashboard if you want to preserve your current one.
7. Reload the frontend. Test spa controls and planner first, then graphs and heating estimates.

## Planner

Tap a row to open its editor popup. The day selection determines `daysMask`. **Opdracht actief** (Command active) enables or disables that entire rule for the selected days. **Verwijderen** (Delete) in the popup deletes that rule.

Hold and double-tap have no action in the dashboard version described here. Use the popup to edit and delete, even if a text hint suggests otherwise.

The ESP web planner can disable a single day. Home Assistant then sees an enabled rule for the other days and a separate disabled rule for that day. Re-enabling the day merges matching rules again.

## Layout and language

Premium, Spa Resort and Spa Console are browser-specific choices. The seven languages follow the Home Assistant language: Dutch, French, German, English, Italian, Spanish and Polish. On phones the planner height follows the number of visible rows, with the system card below it.

## Packages and estimates

Heating estimates need comparable measurement periods and do not guarantee a ready time. Daily totals are calculated in Home Assistant and may differ from ESP counters. If your weather entity has a different name, update its reference in the package. Do not create duplicate sensors if the packages are already active.
