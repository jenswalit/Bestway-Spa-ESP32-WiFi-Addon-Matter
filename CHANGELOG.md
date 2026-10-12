# Wijzigingen

[English](CHANGELOG_EN.md)

## Documentatie — 2026-10-11

- Handleidingen bijgewerkt: bij een nieuwe module verschijnt LazySpaSetup na ongeveer 45 seconden.
- GitHub-uitleg en releasetekst bijgewerkt met een toegankelijke, algemene uitleg, met directe links naar de kant-en-klare OTA-firmware.
- Oude buildstatus en verwijzingen naar nog te uploaden hardwarebestanden opgeruimd. De firmware blijft 5.0.1.

## 5.0.1 — 2026-10-10

- Firmware- en webversie 5.0.1; Matter softwareversienummer verhoogd van 5001 naar 5002. Iedere firmwareupdate krijgt voortaan een nieuwe versie.
- Demo-doeltemperatuur is direct geldig en zichtbaar; +/- werkt ook met een gekozen 4-wire-model zonder Godmode.
- Hardwarebeperkingen voor temperatuur, verwarming en Power worden alleen in hardwaremodus toegepast (web, MQTT en Matter).
- Bouwinstructies beschrijven de lokale Python 3.14.4-omgeving, de `spa`-alias en het opnieuw configureren na een Python-wissel. GitHub Actions blijft op Python 3.12.

## 2026-10-06

- Engelse gebruikershandleiding toegevoegd, inclusief inhoudsopgave, screenshots en montagefoto's.

- Documentatie gericht op installatie en gebruik door nieuwe gebruikers.
- Webplanner: een dag uitschakelen laat de overige dagen actief.
- Een uitgeschakelde dag blijft als aparte plannerregel zichtbaar.
- Bij opnieuw inschakelen worden overeenkomstige regels samengevoegd.
- Maximaal 24 plannerregels; bij onvoldoende ruimte blijft de planning ongewijzigd.

## 2026-10-05 — Handleiding 0.4

- Montagefoto's en stappen voor aansluiting tussen pomp en display.
- Uitleg over WiFi, Matter, Home Assistant en toegang buitenshuis.

## Aanwezige functies

Live webbediening via WebSocket, MQTT, OTA, automatische ontgrendeling,
unieke Matter-onboardinggegevens per module, meerdere Matter-koppelingen,
planner, historie en Home Assistant-dashboard. Debuglogging staat standaard uit.

