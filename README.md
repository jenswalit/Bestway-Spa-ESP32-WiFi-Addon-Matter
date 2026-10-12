# Bestway Spa ESP32 WiFi Add-on with Matter

**Nederlands** | [English](README_EN.md)

<img src="docs/assets/ptw-logo.png" alt="PtW Spa Control" width="160">

Bedien je Bestway Lay-Z-Spa met je telefoon, Home Assistant, Google Home of
Apple Home. Deze ESP32-C6-module wordt tussen de pomp en het originele display
geplaatst. Het fysieke display blijft bruikbaar.

Met dit project kun je de spa makkelijk vanaf je telefoon en via Home Assistant of Matter bedienen. De werkende opstelling is een ESP32-C6-DevKitC-1-N8 met een MIAMI2021 (6-wire Type1).

Dit is een hobbyproject, gebaseerd op het werk van visualapproach. Het is geen officiële Bestway WiFi-module.

## Kies je startpunt

| Wat heb je? | Wat doe je eerst? |
|---|---|
| Een module waarop de firmware al staat | Sluit de module aan en volg de gebruikershandleiding. Zelf bouwen is niet nodig. |
| Een werkende module die je wilt bijwerken | Download het kant-en-klare OTA-bestand van de release en upload het via de webinterface. |
| Een lege ESP32-C6 | Volg de bouwinstructies en flash de eerste keer via USB. |
| Je wilt de module zelf maken | Bekijk de hardware-informatie en het V2M-hardwarepakket. Controleer de verificatiestatus vóór bestellen; de complete assemblage-onderdelenlijst wordt nog afgerond. |

## Firmware downloaden

De kant-en-klare firmware **v5.0.1** staat bij de [release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/tag/v5.0.1). Download **Bestway_Matter_v5.bin** en kies dit bestand op de OTA-pagina van je module. De webinterface zit al in dit bestand; je hoeft niets apart te installeren of zelf te bouwen voor een OTA-update.

De firmware is ook op GitHub gebouwd en de checksum van de download is gecontroleerd. De lokale build met Python 3.14.4 werkt ook en v5.0.1 is op de module in gebruik. In deze versie is de temperatuurbediening in demomodus opgelost.

Voor een lege ESP is er nog geen complete USB-flashdownload. Daarvoor gebruik je de [bouwinstructies](docs/BUILD_NL.md). Een broncode-ZIP is bedoeld om zelf te bouwen.

De [bouwinstructies](docs/BUILD_NL.md#python-314-kiezen) beschrijven nu ook de
lokale Python 3.14.4-omgeving en de bijgewerkte `spa`-alias. De automatische
GitHub-build gebruikt nog Python 3.12; voor een werkende module hoef je Python
niet te installeren.

Heb je een werkende module? Begin met de
**[gebruikershandleiding (Nederlands)](docs/PtW_Spa_Control_Handleiding.pdf)**
of de **[Engelse handleiding](docs/PtW_Spa_Control_User_Manual_EN.pdf)**.
Die beschrijft stap voor stap:

1. De module aansluiten op een geschikte pomp.
2. Bij een nieuwe module ongeveer 45 seconden wachten op **LazySpaSetup**, verbinden en je thuis-WiFi instellen.
3. De webinterface openen en de spa bedienen.
4. Koppelen met Matter en een tweede ecosysteem toevoegen.
5. Optioneel: Home Assistant, het spa-dashboard en toegang buitenshuis instellen.

Home Assistant is optioneel. De webinterface en rechtstreekse Matter-bediening
kun je ook zonder Home Assistant gebruiken.

## Wat kun je ermee?

- Spa aan/uit, filter, verwarming, bubbels en doeltemperatuur bedienen.
- HydroJets bedienen als je model die ondersteunt.
- Dagplanningen maken en bekijken.
- Temperatuur, activiteit, historie en geschat energieverbruik bekijken.
- Firmware bijwerken via de webinterface met OTA.
- Koppelen met meerdere Matter-ecosystemen, zonder bestaande koppelingen te wissen.
- Een Home Assistant-dashboard gebruiken met drie layouts en zeven talen.

## Handleidingen

| Wat wil je doen? | Lees dit |
|---|---|
| Module installeren en alle pagina's leren gebruiken | [Nederlands](docs/PtW_Spa_Control_Handleiding.pdf) / [English](docs/PtW_Spa_Control_User_Manual_EN.pdf) |
| Aansluitingen controleren | [Hardware en aansluiting](docs/HARDWARE_NL.md) |
| Eerste of extra Matter-koppeling maken | [Matter koppelen](docs/MATTER_NL.md) |
| Spa Control via MQTT koppelen | [MQTT instellen](docs/MQTT_NL.md) / [English](docs/MQTT_EN.md) |
| Home Assistant-dashboard installeren | [Home Assistant](home-assistant/README.md) |
| Zelf firmware bouwen | [Bouwinstructies voor ontwikkelaars](docs/BUILD_NL.md) |

Controleer vóór montage of je pomp, kabels en module bij elkaar passen. De werkende
opstelling is een **MIAMI2021 met 6-wire Type1** en een
**ESP32-C6-DevKitC-1-N8**. De aansluitfoto's zijn geen universeel schema voor
alle Bestway-modellen.

## Broncode

`firmware/` bevat de firmware en webinterface. `home-assistant/` bevat het dashboard,
scripts en packages. `docs/` bevat de handleidingen. Tests en informatie voor
bijdragers staan in `tests/`, `tools/` en [CONTRIBUTING.md](CONTRIBUTING.md).

Zie ook de [wijzigingen](CHANGELOG.md).

## Herkomst en licentie

Dit project is gebaseerd op
[WiFi-remote-for-Bestway-Lay-Z-SPA van visualapproach](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA).
Dank aan visualapproach en de bijdragers voor het oorspronkelijke werk.

Deze versie is aangepast voor de ESP32-C6 met ESP-IDF, ESP-Matter en
het bijbehorende Home Assistant-dashboard. Het is een afzonderlijk project;
wijzigingen en ondersteuning vallen niet onder het oorspronkelijke project.

De projectsoftware wordt beschikbaar gesteld onder de **GNU General Public
License versie 3 (GPL-3.0)**. Zie [LICENSE](LICENSE) voor de volledige tekst.
Externe componenten behouden hun eigen licenties. Zie ook
[herkomst en licenties](docs/HERKOMST_EN_LICENTIES.md).
