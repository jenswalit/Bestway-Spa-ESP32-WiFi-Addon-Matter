# Bestway Spa ESP32 WiFi Add-on with Matter

**Nederlands** | [English](README_EN.md)

<img src="docs/assets/ptw-logo.png" alt="PtW Spa Control" width="160">

Bedien je Bestway Lay-Z-Spa met je telefoon, Home Assistant, Google Home of
Apple Home. Deze ESP32-C6-module wordt tussen de pomp en het originele display
geplaatst. Het fysieke display blijft bruikbaar.

Dit is een onafhankelijk hobbyproject, geen officiële Bestway WiFi-module.

## Kies je startpunt

| Wat heb je? | Wat doe je eerst? |
|---|---|
| Een module waarop de firmware al staat | Sluit de module aan en volg de gebruikershandleiding. Zelf bouwen is niet nodig. |
| Een lege module en geschikte kant-en-klare firmware | Flash eerst de firmware via USB. Zelf bouwen is niet nodig als er een passend firmwarebestand beschikbaar is. |
| Een lege module en alleen de broncode | Installeer de bouwomgeving, bouw de firmware en flash de module via USB. |
| Je wilt de module zelf maken | Bekijk de hardware-informatie. Schema, Gerber-bestanden en een complete onderdelenlijst worden later toegevoegd. |

Deze repository bevat broncode. Dat betekent niet automatisch dat er ook een
kant-en-klaar firmwarebestand beschikbaar is. Een GitHub-broncode-ZIP kun je niet
rechtstreeks op een ESP flashen. Zie de [bouwinstructies](docs/BUILD_NL.md) als je
zelf wilt bouwen.

Heb je een werkende module? Begin met de
**[gebruikershandleiding (PDF)](docs/PtW_Spa_Control_Handleiding.pdf)**.
Die beschrijft stap voor stap:

1. De module aansluiten op een geschikte pomp.
2. Verbinden met het WiFi-toegangspunt van de module en je thuis-WiFi instellen.
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
| Module installeren en alle pagina's leren gebruiken | [Gebruikershandleiding](docs/PtW_Spa_Control_Handleiding.pdf) |
| Aansluitingen controleren | [Hardware en aansluiting](docs/HARDWARE_NL.md) |
| Eerste of extra Matter-koppeling maken | [Matter koppelen](docs/MATTER_NL.md) |
| Home Assistant-dashboard installeren | [Home Assistant](home-assistant/README.md) |
| Zelf firmware bouwen | [Bouwinstructies voor ontwikkelaars](docs/BUILD_NL.md) |

Controleer vóór montage of je pomp, kabels en module bij elkaar passen. De
gedocumenteerde testopstelling is een **MIAMI2021 met 6-wire Type1** en een
**ESP32-C6-DevKitC-1-N8**. De aansluitfoto's zijn geen universeel schema voor
alle Bestway-modellen.

## Broncode

`firmware/` bevat de firmware en webinterface. `home-assistant/` bevat het dashboard,
scripts en packages. `docs/` bevat de handleidingen. Tests en informatie voor
bijdragers staan in `tests/`, `tools/` en [CONTRIBUTING.md](CONTRIBUTING.md).

Zie ook de [wijzigingen](CHANGELOG.md) en [licentie-informatie](docs/HERKOMST_EN_LICENTIES.md).
