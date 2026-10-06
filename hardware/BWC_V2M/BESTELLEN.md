# De BWC V2M-print zelf bestellen

[English](ORDERING_EN.md) · [Hardwareoverzicht](README.md)

## Bestelstatus

**PCB-bron en Gerber gecontroleerd op baansegmenten en boringen; DRC en fysieke printtest nog niet vastgelegd.**

De export van 6 oktober 2026 bevat twee koperlagen, soldeermaskers, opdruk, printomtrek en boorbestanden. De outline meet 98,1964 × 64,6684 mm; er zijn vier bevestigingsgaten van 4,064 mm. De vijf vias horen bij de nieuwste PCB-bron in deze map.

Alle 220 rechte baansegmenten, 140 componentboringen, 5 via-boringen en 4 bevestigingsgaten zijn vergeleken met de Gerber. De 140 pin-netverbindingen passen bij het schema. De volledige geometrie en connectiviteit van kopervlakken is niet gecontroleerd.

Voer vóór bestellen een DRC uit in EasyEDA en controleer de Gerberweergave. Een eventuele ontwerpfout blijft mogelijk zolang de print niet fysiek is getest.

## Wat bestel je?

Een **kale printplaat**. De ESP32-C6, levelshifter, weerstanden, connectoren en andere onderdelen moet je apart aanschaffen en monteren. Dit Gerberbestand is geen firmware en geen compleet pakket voor automatische assemblage. Een definitieve onderdelenlijst en montagehandleiding volgen afzonderlijk.

## Bestelstappen

1. Open de hardwarepagina op GitHub en controleer de bestelstatus en printversie.
2. Download alleen [BWC_SPA_CONTROL_V2M_2026-10-06.zip](gerber/BWC_SPA_CONTROL_V2M_2026-10-06.zip) via de downloadknop bij het bestand. Pak deze ZIP **niet** uit. Upload niet de ZIP van de hele repository of van het hardwarepakket.
3. Open bijvoorbeeld [JLCPCB](https://jlcpcb.com/) en kies **Order Now / Instant Quote**. Maak zo nodig een account aan.
4. Klik **Add your Gerber file** en upload de Gerber-ZIP. De fabrikant analyseert de lagen en afmetingen.
5. Controleer **2 koperlagen** en ongeveer **98,20 × 64,67 mm**. Kies één ontwerp en losse printplaten; stel het gewenste aantal in.
6. Controleer in **Gerber Viewer** beide zijden, printomtrek, bevestigingsgaten en de twee rijen ESP32-headergaten. Ga bij onverwachte gaten, ontbrekende lagen of een andere outline terug naar het ontwerp.
7. Controleer de gewenste printdikte, het kopergewicht en de oppervlakteafwerking. Deze bestelkeuzes zijn nog niet bevestigd; Gerbers alleen leggen deze keuzes niet volledig vast. Schakel PCB Assembly uit voor een kale printbestelling.
8. Controleer aantal, verzendadres en totaalprijs inclusief verzendkosten en eventuele belastingen. Plaats daarna zelf de bestelling. Een fabrikantcontrole vervangt geen elektrische ontwerpcontrole.

Officiële uitleg: [JLCPCB — How do I place a PCB order?](https://jlcpcb.com/help/article/how-do-i-place-an-order).

## De link delen

Deel de [projectpagina](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter) of, nadat deze bestanden zijn geüpload, de [hardwaremap](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/tree/main/hardware/BWC_V2M). Bij een openbare repository kunnen bezoekers de bestanden zonder GitHub-account downloaden.

Een EasyEDA-editorlink is alleen bruikbaar voor anderen wanneer zij toegang tot het project hebben. GitHub met de Gerber en deze uitleg is de duidelijkste start voor iemand die alleen een print wil bestellen.
