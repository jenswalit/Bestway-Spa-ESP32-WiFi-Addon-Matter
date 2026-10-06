# Bestway Spa ESP32 WiFi Add-on with Matter

<img src="docs/assets/ptw-logo.png" alt="PtW Spa Control" width="160">

Een onafhankelijke WiFi-uitbreiding voor Bestway Lay-Z-Spa op de **ESP32-C6**.
Dit is een hobbyproject en geen officiële Bestway WiFi-module.

De module komt tussen de pompprint en het originele display. Je kunt de spa lokaal
bedienen via de webinterface, via MQTT in Home Assistant en via Matter in meerdere
ecosystemen. Bestaande functies blijven op de echte pompstatus gebaseerd.

## Begin hier

- **Module installeren en gebruiken:** [gebruikershandleiding PDF](docs/PtW_Spa_Control_Handleiding.pdf).
- **Firmware bouwen en via OTA bijwerken:** [bouwinstructies](docs/BUILD_NL.md).
- **Onze HA-pagina installeren:** [Home Assistant](home-assistant/README.md).
- **Dit pakket naar GitHub zetten:** [uploadstappen voor Ubuntu](docs/GITHUB_UPLOAD_NL.md).
- **Wijzigingen:** [changelog](CHANGELOG.md).

## Functies

- WebSocket voor live status en bediening, zonder onnodige automatische HTTP-polling.
- Power, filter, verwarming, bubbels, doeltemperatuur en HydroJets bij geschikte modellen.
- Automatisch ontgrendelen wanneer een functie dat nodig heeft.
- Planner met maximaal 24 regels en bediening per dag.
- Historie, geschat energieverbruik, onderhoudsherinneringen en OTA.
- Unieke Matter-onboardinggegevens per module, bewaard over OTA-updates.
- Meerdere Matter-fabrics en tijdelijk koppelvenster voor extra controllers.
- Home Assistant-dashboard met Premium, Spa Resort en Spa Console, plus zeven talen.
- Opwarmschatting met Buienradar en dagtotalen voor verwarming, filter en bubbels.
- Debug/live logging standaard uit; logbuffer alleen wanneer logging wordt aangezet.

## Huidige basis

De planner-dagfix is op **6 oktober 2026 door de eigenaar als werkend bevestigd**.
De firmware blijft de bestaande projectversie `5.0.0` gebruiken. De repository-start
is geen nieuwe OTA-versie: deze indeling verandert de werkende broncode niet.

De aangeleverde bouwconfiguratie en dependency lock vermelden **ESP-IDF 5.5.5**.
ESP-Matter en connectedhomeip zijn externe SDK's; zie [versies](docs/BUILD_NL.md).
Er is hier geen volledige ESP-IDF/Matter-build uitgevoerd. De gerichte C++- en
JavaScript-tests voor de planner zijn wel uitgevoerd.

## Indeling

| Map of bestand | Inhoud |
|---|---|
| `firmware/` | ESP-IDF-project met embedded webinterface en werkende configuratie |
| `home-assistant/` | Dashboard, JavaScript, planner scripts en packages |
| `docs/` | Handleiding, bouwen, hardware, Matter en GitHub-upload |
| `tests/` | Gerichte hosttests met ESP/NVS-stubs |
| `tools/verify.sh` | Lokale broncontrole en planner-tests |
| `CHANGELOG.md` | Wijzigingen en bevestigde basis |

Het ZIP-pakket behoudt `managed_components` als aangeleverde snapshot. Git negeert
die gegenereerde downloads; een nieuwe checkout haalt ze via de componentmanager
op met de versies uit `dependencies.lock`. De aanwezige hoofdlicenties zijn ook
gekopieerd naar `docs/third-party/`. Oude backups, logs en verouderde fase-README's
zijn niet meegenomen.
De bewaarde `sdkconfig` bevat de gecontroleerde bouwinstellingen; echte netwerk- en
modulegegevens worden tijdens gebruik ingesteld en horen niet in Git.

## Aansluiting

**Pompprint → CIO-zijde van module → DSP-zijde → origineel display.**
Haal de netstekker uit het stopcontact voordat je de pomp opent. Gebruik de juiste
kabels voor jouw uitvoering. De montagefoto's in de handleiding tonen de besproken
pomp, geen universele aansluiting voor alle modellen. Zie [hardware](docs/HARDWARE_NL.md).

## Bijdragen en uitbrengen

Werk voor veranderingen op een branch, bouw en test op de spa en documenteer de
uitkomst. Gebruik OTA voor normale updates. Een factory reset of NVS-wisactie is
geen normale update- of extra Matter-koppelstap. Zie [bijdragen](CONTRIBUTING.md).

Dit pakket kiest geen nieuwe algemene open-source-licentie namens de maker.
Lees [herkomst en licenties](docs/HERKOMST_EN_LICENTIES.md) voordat je het project
openbaar verspreidt of er een algemene licentie aan toevoegt.
