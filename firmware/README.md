# ESP32-C6 firmware

[English](README_EN.md)

Hier staat de broncode van de ESP32-C6-firmware **v5.0.1**, inclusief de planner-dagfix en de verbeterde temperatuurbediening in demomodus.

Wil je alleen een bestaande module bijwerken? Het kant-en-klare `Bestway_Matter_v5.bin` staat bij de [laatste release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/latest). Upload dat op de OTA-pagina; zelf bouwen hoeft dan niet.

Bouwen vanuit deze map, met jouw bestaande ESP-IDF en ESP-Matter omgeving:

```bash
idf.py build
```

De output is `build/Bestway_Matter_v5.bin`. Update normaal via de bestaande
OTA-pagina. Volledige stappen en SDK-versies: [BUILD_NL.md](../docs/BUILD_NL.md).

`components/web_server/web/` bevat de embedded webassets. De bewaarde sdkconfig,
sdkconfig.defaults, partities en dependency lock horen bij de werkende basis.
Deze GitHub-indeling verandert de firmwarebronnen niet.
