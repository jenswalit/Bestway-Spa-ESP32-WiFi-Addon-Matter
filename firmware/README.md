# ESP32-C6 firmware

[English](README_EN.md)

Dit is de werkende broncode met de op 6 oktober 2026 bevestigde planner-dagfix.

Bouwen vanuit deze map, met jouw bestaande ESP-IDF en ESP-Matter omgeving:

```bash
idf.py build
```

De output is `build/Bestway_Matter_v5.bin`. Update normaal via de bestaande
OTA-pagina. Volledige stappen en SDK-versies: [BUILD_NL.md](../docs/BUILD_NL.md).

`components/web_server/web/` bevat de embedded webassets. De bewaarde sdkconfig,
sdkconfig.defaults, partities en dependency lock horen bij de werkende basis.
Deze GitHub-indeling verandert de firmwarebronnen niet.
