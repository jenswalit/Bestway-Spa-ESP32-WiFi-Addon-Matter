# Firmware bouwen — voor ontwikkelaars

[English](BUILD_EN.md)

Een geprogrammeerde module gebruiken? Volg de [gebruikershandleiding](PtW_Spa_Control_Handleiding.pdf). Bouwen is daarvoor niet nodig.

## Vereiste bouwomgeving

| Onderdeel | Vastgelegde basis |
|---|---|
| Chip | ESP32-C6, flash 8 MB |
| ESP-IDF | 5.5.5 volgens sdkconfig en dependencies.lock |
| ESP-Matter | ff9f07ecb0bf01d7a2db5428149ef16bf601774c |
| connectedhomeip | 539342f32d5f4dc93761c2f9325afe29270068f1 |
| Projectversie | 5.0.0 |

Installeer ESP-IDF en ESP-Matter en gebruik de SDK-versies uit deze tabel.
De SDK's staan buiten deze repository. Controleer de versies van je installatie:

```bash
idf.py --version
git -C ~/esp/esp-matter rev-parse HEAD
git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip rev-parse HEAD
```

De onderstaande voorbeelden gebruiken ESP-IDF in
`~/.espressif/release-v5.5/esp-idf` en ESP-Matter in `~/esp/esp-matter`.
Pas de paden aan als je de SDK's elders hebt geïnstalleerd.
De dependency-componenten worden bij de eerste build gedownload volgens
`dependencies.lock`; daarvoor is netwerktoegang nodig.

## Project downloaden

Download en pak de broncode uit, of kloon de repository:

```bash
mkdir -p ~/projects
git clone https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter.git ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
```

## Bouwen

Open een terminal waarin je IDF/Matter-omgeving actief is. Zo nodig:

```bash
source ~/.espressif/release-v5.5/esp-idf/export.sh
source ~/esp/esp-matter/export.sh
cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter/firmware
idf.py build
```

Het project staat in `firmware/`. De meegeleverde `sdkconfig` bevat de projectinstellingen. Verwijder hem niet als normale bouwstap en voer niet onnodig set-target uit.
Als jouw eigen IDF-versie anders is, controleer dat eerst; de mapnaam release-v5.5
alleen zegt niet welke patchversie is geïnstalleerd.

## OTA

Na een geslaagde build is het appbestand:

`firmware/build/Bestway_Matter_v5.bin`

Upload dat via de bestaande OTA-pagina van de module. Een broncode-ZIP, bootloader
of partition-tablebestand is geen appbestand voor deze OTA-pagina. Laat voeding en
WiFi tijdens de update beschikbaar. Factory reset, erase-flash en NVS wissen zijn
geen onderdeel van deze stap.

## Controles bij firmwarewijzigingen

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

Deze instructies veronderstellen een geïnstalleerde ESP-IDF/Matter-bouwomgeving;
ze zijn geen complete SDK-installatiehandleiding.
