# BWC PCB V2M — ESP32-C6

[English](README_EN.md)

Addonprint voor een ESP32-C6-DevKitC-1-N8, voor gebruik met de firmware uit deze repository. De module komt tussen de pomp en het fysieke display van de spa.

Dit is een onafhankelijk hobbyproject, geen officiële Bestway Wi-Fi-module.

## Beschikbare bestanden

| Bestand | Gebruik |
| --- | --- |
| [PCB_BWC_V2M.json](easyeda/PCB_BWC_V2M.json) | Bewerkbare PCB in EasyEDA Standard |
| [SCH_BWC_V2M.json](easyeda/SCH_BWC_V2M.json) | Bijpassend elektrisch schema |
| [PCBLIB_BWC_ESP32C6_V2M.json](easyeda/PCBLIB_BWC_ESP32C6_V2M.json) | ESP32-C6-footprint met 32 headerpads |
| [Schema.pdf](Schema.pdf) | Leesbaar elektrisch overzicht |

## Print bestellen

De [Gerber-ZIP van 6 oktober 2026](gerber/BWC_SPA_CONTROL_V2M_2026-10-06.zip) en de bijbehorende nieuwste PCB-bron zijn opgenomen. Zie [de Nederlandse besteluitleg](BESTELLEN.md).

**Status: baansegmenten en boorbestanden vergeleken met de PCB-bron; DRC en fysieke printtest nog niet vastgelegd.** De volledige geometrie en verbindingen van de kopervlakken zijn niet gecontroleerd. Controleer de DRC en de Gerberweergave vóór een bestelling.

Een kale printbestelling bevat geen ESP32, levelshifter of overige onderdelen. Een complete assemblage-BOM en montage-instructie zijn nog niet gepubliceerd.

[EasyEDA-project](https://easyeda.com/editor#project_id=5c570d72ac544b518ed51a159b791d9f) — toegang voor andere gebruikers hangt af van de zichtbaarheid van het project.

## Openen in EasyEDA Standard

1. Download de gewenste JSON via de Raw/downloadknop op GitHub, of download de hele repository als ZIP.
2. Kies in EasyEDA Standard **File → Open → EasyEDA** en open het bestand.
3. Houd één bijpassende schemaversie en PCB-versie in hetzelfde project. De footprintbibliotheek kan afzonderlijk worden geïmporteerd.
4. U1 is in deze PCB gekoppeld aan de ESP32-C6-footprint. De oude losse ESP-headerpads zijn vervangen zonder wijziging van de padposities.

## Aansluitingen voor de huidige firmware

| Functie | ESP32-C6 GPIO |
| --- | --- |
| CIO DATA — pompzijde | 23 |
| CIO CLK — pompzijde | 22 |
| CIO CS — pompzijde | 21 |
| DSP DATA — displayzijde | 20 |
| DSP CLK — displayzijde | 19 |
| DSP CS — displayzijde | 18 |
| AUDIO | 11 |
| Temperatuurreserve | 0 |
| RGB-statusled op de DevKit | 8 |

GPIO10 is op deze print ook naar een extra aansluiting uitgevoerd. Dit is geen van de hierboven gebruikte spa-buspinnen.

U2 en U3 zijn alternatieve levelshiftermodules uit het ontwerp. De definitieve assemblage-BOM moet aangeven welke uitvoering wordt geplaatst. R10 is een 4,7 kΩ pull-up naar 3V3.

## Uitgevoerde controle

De controle betreft de aangeleverde PCB `PCB_BWC_SPA_CONTROL_V2M_2026-10-06.json` en Gerber-export van 6 oktober 2026:

- Alle 140 componentpin-netverbindingen en 27 componentkoppelingen komen overeen met het schema.
- De GPIO-toewijzing voor CIO, DSP, audio en reserveaansluitingen komt overeen.
- Alle 220 rechte segmenten uit de 115 koperbanen zijn in de overeenkomstige Gerber-koperlagen teruggevonden, inclusief baanbreedte.
- Alle 140 componentboringen en 5 via-boringen komen overeen qua positie en diameter, binnen 0,002 mm vergelijkingstolerantie.
- De 4 bevestigingsgaten komen overeen; diameter 4,064 mm.
- De ZIP is zonder archieffouten te lezen en is ongewijzigd overgenomen.

De vijf vias horen bij het nieuwste ontwerp. De eerdere ontwerpversie met zeven vias is vervangen; die afwijking is hiermee verklaard.

Zie [CONTROLE.txt](gerber/CONTROLE.txt) voor de technische controle. Deze vergelijking is geen volledige controle van kopervlakken, elektrische koperconnectiviteit of afstanden. Een EasyEDA-DRC en fysieke test van een gefabriceerde V2M-print zijn nog niet vastgelegd.

## Herkomst en licentie

Gebaseerd op [visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA).

De softwarelicentie en algemene herkomstinformatie staan in de [LICENSE](../../LICENSE) en [herkomstinformatie](../../docs/HERKOMST_EN_LICENTIES.md) van deze repository. Er is in dit hardwarepakket nog geen afzonderlijke hardwarelicentie vastgesteld; de softwarelicentie wordt hier niet automatisch tot de PCB-ontwerpbestanden uitgebreid.
