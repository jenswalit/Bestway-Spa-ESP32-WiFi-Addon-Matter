# Onderdelenlijst — BWC V2M / ESP32-C6

**Nederlands** | [English](BOM_EN.md) · [Hardwareoverzicht](README.md)

Deze **ontwerp-BOM** is afgeleid van `PCB_BWC_SPA_CONTROL_V2M_2026-10-06.json`, de versie met vijf vias. De waarden en aantallen komen uit de ontwerpgegevens. Dit is een lijst voor handmatige bouw; nog geen volledig bevestigde winkelmand of automatische assemblage-BOM.

Download: [gegroepeerde BOM.csv](BOM.csv) · [alle 27 componentposities met brongegevens](BOM_Source.csv).

## Basis en passieve onderdelen

| Referentie | Aantal in ontwerp | Onderdeel / waarde | Montage |
|---|---:|---|---|
| PCB | 1 | BWC SPA CONTROL V2M, 2 lagen | Kale print; zie de [besteluitleg](BESTELLEN.md) |
| U1 | 1 | ESP32-C6-DevKitC-1-N8 | Complete DevKit; 2 × 16 pinnen, 2,54 mm steek, 22,86 mm rijafstand |
| R2–R8 | 7 | 560 Ω | Axiaal, doorsteek; padafstand 5,25 mm |
| R10 | 1 | 4,7 kΩ | Axiaal, doorsteek; padafstand 7,30 mm; GPIO0 pull-up naar 3V3 |
| C2–C8 | 7 | 220 pF | Keramisch, doorsteek; steek 5,00 mm |

Er zijn in dit ontwerp **geen R9 en geen C1**. Weerstandsvermogen, tolerantie, condensatoruitvoering en fysieke pasvorm moeten bij de definitieve onderdelenkeuze worden bevestigd; de CSV vermeldt de oorspronkelijke bibliotheeknummers zonder ze als gegarandeerde vervangers te presenteren.

## Levelshifter — kies één uitvoering

| Referentie | Aantal | Uitvoering | Footprint |
|---|---:|---|---|
| U2 | 1 als U2 wordt gekozen | 8-kanaals bidirectionele MOSFET-levelshiftermodule, 3,3 ↔ 5 V | 2 × 10 pinnen; 2,54 mm steek; 11,43 mm rijafstand |
| U3 | 1 als U3 wordt gekozen | TXS0108E-levelshiftermodule | 2 × 10 pinnen; 2,54 mm steek; 12,70 mm rijafstand |
| R1 | 1, alleen bij U3 | 10 kΩ axiale weerstand | Padafstand 5,25 mm; ontwerptekst: “10K only with U3” |

**Plaats U2 of U3, niet beide.** Controleer de volledige pinvolgorde en maatvoering van de gekozen module aan de hand van het schema; “8-kanaals levelshifter” alleen is onvoldoende om een passend exemplaar te kiezen.

De U3-bibliotheek noemt `TXS0108EPWR` / `C17206`. Dat verwijst naar de **losse TSSOP-chip**, niet naar de complete module met headerpinnen die deze PCB verwacht. Gebruik dat nummer dus niet als bestelnummer voor U3. Het exacte bestelnummer van beide modulevarianten is nog niet bevestigd.

R1 is volgens de PCB tussen GND en 3V3 aangesloten; deze BOM noemt hem daarom geen OE-pulldown. De OE-pin van U3 staat in het ontwerp aan 3V3.

## U2: generiek moduletype, geen verplicht merk

Het bedoelde U2-type is de **8-kanaals bidirectionele 3,3 V ↔ 5 V logic-level-convertermodule met afzonderlijke MOSFET-kanalen**, zoals het aangeleverde voorbeeld. Het merk, de kleur van de print en de verkoper zijn niet voorgeschreven. Gebruik als zoekomschrijving: `8 channel bidirectional MOSFET logic level converter 3.3V 5V`.

De exacte transistorcode van het voorbeeld is niet bevestigd. Controleer vóór aanschaf dat de module werkelijk deze mechanische en elektrische aansluitvolgorde heeft:

- Twee rijen van tien pinnen, steek 2,54 mm en rijafstand 11,43 mm.
- Lage zijde op 3V3 (LV), hoge zijde op 5V (HV), gemeenschappelijke GND.
- De kanaal- en voedingspinnen moeten overeenkomen met onderstaande footprintvolgorde. Een andere module met hetzelfde aantal pinnen is niet automatisch passend.

| Footprintpositie | Lage zijde J1 | Hoge zijde J2 |
|---|---|---|
| 1–4 | LV-kanaal 1–4 | HV-kanaal 1–4 |
| 5 | LV / 3V3 | HV / 5V |
| 6 | GND | GND |
| 7–10 | LV-kanaal 5–8 | HV-kanaal 5–8 |

Lees deze posities volgens de nummering in het schema, niet blind van boven naar beneden op een winkelafbeelding. U3 blijft een aparte alternatieve TXS0108E-modulepositie; plaats die niet tegelijk met U2.

## Connectoren en extra aansluitingen

| Referentie | Ontwerpposities | Functie | Geometrie |
|---|---:|---|---|
| H1A, H1B | 2 | CIO / pompzijde, A- en B-aansluiting | 1 × 6 pinnen, steek 2,54 mm |
| H2A, H2B | 2 | DSP / displayzijde, A- en B-aansluiting | 1 × 6 pinnen, steek 2,54 mm |
| J1 | 1 | GND-uitbreiding | 1 × 4 pinnen, steek 2,54 mm |
| J3 | 1 | 3V3-uitbreiding | 1 × 4 pinnen, steek 2,54 mm |
| J2 | 1 | GPIO0 / temperatuurreserve | 1 × 2 pinnen, steek 2,54 mm |
| J4 | 1 | GPIO10 | 1 × 2 pinnen, steek 2,54 mm |

De vier H-connectorposities zijn **aantallen in het ontwerp**. Welke je daadwerkelijk plaatst hangt af van de spa-kabelset. Het definitieve stekkertype, de tegenstekker, oriëntatie en bezetting moeten nog worden bevestigd. De bron noemt Molex `22284060` / `C587040`; dit bewijst niet dat die connector zonder verdere controle op jouw spa-stekker past.

De J-aansluitingen zijn optionele uitbreidingen. “Jumper2” in de bibliotheek betekent hier geen opdracht om pinnen kort te sluiten.

## Apart nodig voor montage

Deze onderdelen hebben geen eigen componentreferentie in de PCB en zijn niet meegerekend in de 27 ontwerpcomponenten:

- Eventueel 2 × 16-polige female headers voor een uitneembare ESP32-C6, afhankelijk van de DevKit-uitvoering en montagehoogte.
- Passende headers of sockets voor de gekozen levelshiftermodule, indien niet meegeleverd.
- Geschikte pomp-/displaykabels met passende stekkers en gecontroleerde pinvolgorde.
- Behuizing, bevestigingsmateriaal, afstandhouders en trekontlasting passend bij de montageplek.

Er staat geen externe RGB-led of buzzer in de PCB-BOM. GPIO8 gebruikt de led op de DevKit; een eventueel apart audio-onderdeel is niet in deze ontwerpbestanden gespecificeerd.

## Controle en bronnen

Alle 27 componentposities zijn opgenomen; gegroepeerde weerstanden en condensatoren zijn op gelijke bronwaarden gecontroleerd. De CSV-bestanden bevatten geen prijzen of voorraadclaims. Een volledige assemblage- en fysieke test van deze V2M-print is nog niet vastgelegd.

Bronidentiteit: [BOM_SOURCE.txt](BOM_SOURCE.txt). Productcontrole: [TI TXS0108EPWR — losse TSSOP-20-chip](https://www.ti.com/product/TXS0108E/part-details/TXS0108EPWR), [LCSC C249163 — 220 pF-condensator](https://www.lcsc.com/product-detail/C249163.html).
