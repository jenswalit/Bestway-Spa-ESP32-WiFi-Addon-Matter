# Firmware bouwen en bijwerken

## Werkende omgeving behouden

| Onderdeel | Vastgelegde basis |
|---|---|
| Chip | ESP32-C6, flash 8 MB |
| ESP-IDF | Aangeleverde sdkconfig en dependencies.lock: 5.5.5 |
| ESP-Matter | ff9f07ecb0bf01d7a2db5428149ef16bf601774c |
| connectedhomeip | 539342f32d5f4dc93761c2f9325afe29270068f1 |
| Projectversie | 5.0.0 |

De twee Matter-commit-ID's zijn door de eigenaar opgegeven; controleer de eigen
checkout met de onderstaande commando's. Verander SDK-versies niet tegelijk met
het verplaatsen van de broncode naar GitHub.

```bash
git -C ~/esp/esp-matter rev-parse HEAD
git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip rev-parse HEAD
```

Deze repository bevat geen complete ESP-IDF- of ESP-Matter-checkout. Gebruik je
bestaande SDK's. De registry-componenten zijn als bestaande snapshot meegenomen;
`dependencies.lock` blijft behouden. Git houdt de gegenereerde map buiten versiebeheer;
bij een nieuwe checkout downloadt de componentmanager de dependencies. Voor die
eerste build is netwerktoegang nodig.

## Bouwen

Open een terminal waarin jouw bestaande IDF/Matter-omgeving actief is. Zo nodig:

```bash
source ~/.espressif/release-v5.5/esp-idf/export.sh
source ~/esp/esp-matter/export.sh
cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter/firmware
idf.py build
```

Het project staat nu in `firmware/`. De bewaarde `sdkconfig` hoort bij de bevestigde
basis. Verwijder hem niet als normale bouwstap en voer niet onnodig set-target uit.
Als jouw eigen IDF-versie anders is, controleer dat eerst; de mapnaam release-v5.5
alleen zegt niet welke patchversie is geïnstalleerd.

## OTA

Na een geslaagde build is het appbestand:

`firmware/build/Bestway_Matter_v5.bin`

Upload dat via de bestaande OTA-pagina van de module. Een broncode-ZIP, bootloader
of partition-tablebestand is geen appbestand voor deze OTA-pagina. Laat voeding en
WiFi tijdens de update beschikbaar. Factory reset, erase-flash en NVS wissen zijn
geen onderdeel van deze stap.

## Controles

- Firmware bouwt zonder fouten en past in de bestaande app-partitie.
- HTTP-server houdt 46 gewone handlers plus 1 WebSocket binnen de limiet van 48.
- Logging staat normaal uit; geen extra permanente logbuffers of CIO-debug.
- WebSocket-bediening, echte pomptarget, MQTT, Matter, OTA en planner werken.
- Dagknop: alleen gekozen dag uit, overige dagen aan, status na herladen correct.
- MQTT plannerstatus en Home Assistant komen overeen.
- Heap in dezelfde toestand vergelijken; Matter-pairing/BLE beïnvloedt de waarde.

## Gerichte hosttests

Vanaf de repository-root:

```bash
bash tools/verify.sh
```

Nodig: Python 3, Node.js en g++ met C++17. Deze tests gebruiken de echte plannercode
met NVS/ESP-stubs. Ze vervangen geen volledige ESP-IDF-build of test op de spa.

Componentmanager: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html
