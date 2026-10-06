# Wijzigingen

## 2026-10-06 — Werkende GitHub-basis

- Eigenaar bevestigt dat de planner-dagfix werkt.
- Webplanner stuurt de gekozen dag mee; andere dagen blijven actief.
- Uitgeschakelde dag blijft zichtbaar als een aparte plannerregel.
- Overeenkomstige regels worden opnieuw samengevoegd bij terug inschakelen.
- Eén opslagbewerking per dagwijziging en één gerichte GET om de webplanner bij te werken.
- Bestaande NVS-, backup- en MQTT-plannerstructuur behouden; maximaal 24 regels.
- Complete broncode georganiseerd in firmware, Home Assistant en documentatie.
- Oude backups/logs/fasedocumentatie uit deze repository-indeling gehouden.
- Geen wijzigingen aan de werkende firmwarecode ten opzichte van de bevestigde dagfix.

## 2026-10-05 — Handleiding 0.4

- Montage tussen originele displaystekker en vrijgekomen pompconnector bevestigd.
- Verwijzingen naar een leverancier verwijderd.
- Onafhankelijk hobbyproject en onofficiële Bestway-uitbreiding duidelijk vermeld.

## Eerdere stabiele basis

WebSocket-first bediening, LowHeap logging, HTTP-handlerfix, automatische unlock,
HA MQTT lock-switch, unieke Matter-credentials en multi-admin behouden.
