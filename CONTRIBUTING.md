# Werken aan dit project

[English](CONTRIBUTING_EN.md)

Maak kleine, gerichte wijzigingen. Benoem het probleem, de wijziging
en de uitgevoerde controles in een pull request. Pas SDK, bus en UI niet tegelijk
zonder concrete noodzaak aan.

De webassets zijn embedded in de firmware. Houd WebSocket-first bediening, MQTT,
Matter multi-admin, OTA, planner en Home Assistant behouden. Logbuffers zijn alleen
nodig wanneer de gebruiker debug/live logging handmatig aanzet.

Voeg geen HTTP-handlers toe zonder de teller te controleren. Verander geen target
naar een verzonnen default na reboot. Gebruik geen factory reset of NVS erase als
algemene foutoplossing. Analyseer bij een fout na OTA eerst het rapport/log.

Voer `bash tools/verify.sh` uit voor wijzigingen aan de planner of webplanner, bouw
vervolgens in de passende ESP-IDF/Matter-omgeving en beschrijf de hardwaretest.
Commit geen persoonlijke exports, Matter-codes, netwerksleutels of flashdumps.
