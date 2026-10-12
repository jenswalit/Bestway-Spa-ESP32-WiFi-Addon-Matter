# Firmware bouwen op GitHub

**Nederlands** | [English](GITHUB_ACTIONS_EN.md)

De firmware is ook op GitHub gebouwd. Zo kun je een kant-en-klaar OTA-bestand downloaden zonder zelf ESP-IDF en ESP-Matter te installeren.

## Meteen bijwerken

De volledige firmwarebuild voor **v5.0.1** is geslaagd: [build #6](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/actions/runs/38127239675).
Het kant-en-klare **Bestway_Matter_v5.bin** staat bij de [release](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases/tag/v5.0.1), samen met de checksum en buildgegevens.

1. Download het .bin-bestand onder **Assets**.
2. Open de OTA-pagina in de webinterface van je module.
3. Kies het bestand en start de update. Houd de voeding en WiFi beschikbaar.
4. Wacht op de herstart en controleer de versie.

De webinterface zit in het firmwarebestand. Bij een normale OTA-update blijven je instellingen en Matter-koppelingen behouden; een factory reset of NVS-wisactie is niet nodig.

Voor een lege ESP32-C6 moet je de eerste keer via USB flashen. Er staat nog geen complete USB-installatieset bij de release; volg daarvoor de [bouwinstructies](BUILD_NL.md).

## Bouwomgeving

- GitHub-runner: Ubuntu 22.04 met Python 3.12.
- ESP-IDF: v5.5.5.
- ESP-Matter: `ff9f07ecb0bf01d7a2db5428149ef16bf601774c`.
- connectedhomeip: `539342f32d5f4dc93761c2f9325afe29270068f1`.
- Target: ESP32-C6 met de meegeleverde `firmware/sdkconfig` en partities.

Lokaal is de firmware gebouwd met Python 3.14.4. Beide builds slagen. De workflow controleert de SDK-revisies en voert de projectcontroles uit voordat hij de firmware bouwt.

## Zelf een GitHub-build starten

De workflow staat al in deze repository. In je eigen fork moet je Actions eventueel eerst inschakelen.

1. Ga naar **Actions**.
2. Kies **Build ESP32-C6 firmware**.
3. Kies **Run workflow → main → Run workflow**.
4. Open de run om de voortgang te volgen.

Wijzigingen aan firmware, tests, `tools/verify.sh` of de buildworkflow starten ook automatisch een build. Alleen documentatie- en hardwarewijzigingen doen dat niet.

De SDK-download en build kunnen een tijd duren. De workflow heeft een limiet van 90 minuten. Bij een fout vind je het log onder de mislukte stap.

## Download van een eigen build

Na een geslaagde run staat onder **Artifacts** de download `Bestway_ESP32C6_OTA_<runnummer>`. Pak die ZIP uit en gebruik het .bin-bestand op de OTA-pagina. De ZIP zelf upload je niet naar de module.

De download bevat:

- `Bestway_Matter_v5.bin`: de firmware met webinterface.
- `SHA256SUMS.txt`: de checksum.
- `build-info.json`: de broncommit, SDK-versies en bestandsgegevens.
- `README.txt`: korte OTA-instructies.

Artifacts blijven normaal 30 dagen beschikbaar en vragen doorgaans een GitHub-aanmelding. Gepubliceerde firmware vind je ook bij [Releases](https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter/releases).

## Releases

De buildworkflow bouwt en bewaart het OTA-bestand. De aparte publicatieworkflow controleert de geslaagde build, broncommit en checksum en zet dat bestand bij de release. Geen van beide workflows werkt je module vanzelf bij; je start OTA zelf in de webinterface.
