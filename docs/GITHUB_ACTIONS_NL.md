# Firmware bouwen op GitHub

**Nederlands** | [English](GITHUB_ACTIONS_EN.md)

GitHub Actions kan deze firmware op een Linux-buildserver bouwen. Je hoeft daarvoor op je eigen computer geen ESP-IDF en ESP-Matter te installeren.

## Vastgelegde basis

- Runner: Ubuntu 22.04.
- ESP-IDF: v5.5.5, zoals de sdkconfig-header en dependencies.lock van de projectkopie aangeven.
- ESP-Matter: `ff9f07ecb0bf01d7a2db5428149ef16bf601774c`.
- connectedhomeip: `539342f32d5f4dc93761c2f9325afe29270068f1`.
- Target: ESP32-C6, met de bestaande `firmware/sdkconfig` en partities.

De workflow controleert of de CHIP-submodule exact bij deze basis past en stopt bij een afwijking. Hij installeert alleen de ESP32-C6 IDF-tools en slaat het bouwen van Matter hosttools over. De firmwarebronnen worden niet aangepast.

## Installeren in de bestaande lokale GitHub-map

Download `Bestway_GitHub_Actions_ESP32C6.zip` naar Downloads. Voer uit vanuit de lokale repository en stop bij een fout:

```bash
git pull --ff-only
unzip -o ~/Downloads/Bestway_GitHub_Actions_ESP32C6.zip -d .
git add -- .github/workflows/build-esp32c6.yml docs/GITHUB_ACTIONS_NL.md docs/GITHUB_ACTIONS_EN.md
git diff --cached --stat
git commit -m "Add GitHub Actions ESP-IDF and ESP-Matter firmware build"
git push origin main
```

## Build starten

1. Open je repository op GitHub.
2. Ga naar **Actions** en schakel workflows in als GitHub daarom vraagt.
3. Kies **Build ESP32-C6 firmware**.
4. Kies **Run workflow → main → Run workflow**.
5. Open de run om voortgang en foutmeldingen te bekijken.

De eerste push van de workflow start ook een build. Latere wijzigingen aan firmware, tests, `tools/verify.sh` of deze workflow op `main` starten automatisch een build. Alleen documentatie- en hardwarewijzigingen starten geen firmwarebuild.

De eerste build downloadt meerdere SDK's en dependencies en kan lang duren. Een maximum van 90 minuten voorkomt dat een run eindeloos blijft lopen. Bij een rode run: open de mislukte stap en deel de eerste relevante foutmelding. Wijzig geen SDK-versies op goed geluk.

## Firmware downloaden

Na een groene build staat onderaan de run bij **Artifacts** een download met naam `Bestway_ESP32C6_OTA_<runnummer>`. Artifactdownloads vereisen doorgaans een GitHub-aanmelding. De workflow bewaart ze 30 dagen, of korter als je repositorylimiet dat voorschrijft.

Pak de download uit. Hij bevat:

- `Bestway_Matter_v5.bin`: appbestand voor OTA.
- `SHA256SUMS.txt`: checksum.
- `build-info.json`: broncommit, SDK-versies en bestandsgegevens.
- `README.txt`: korte gebruiksuitleg.

Open de bestaande OTA-pagina van de module en kies **het uitgepakte .bin-bestand**, niet de ZIP. Houd voeding en WiFi beschikbaar. Een normale OTA-update vraagt geen factory reset en geen NVS-wisactie.

Dit artifact bevat geen complete USB-installatieset voor een lege ESP. Eerste installatie via USB volgt de [bouwinstructies](BUILD_NL.md). Een groene build bewijst compilatie, geen fysieke test op een spa.

## Status en publicatie

De workflow en shellsyntaxis zijn lokaal gecontroleerd. Een volledige ESP-IDF/Matter-build is in deze werkomgeving niet uitgevoerd. De eerste GitHub-run moet de SDK-installatie en volledige compilatie bevestigen.

De workflow werkt met alleen leestoegang tot de repository. Hij publiceert geen GitHub Release en voert geen automatische OTA uit. Voor langdurig openbaar beschikbare firmware kan na succesvolle build en hardwaretest een Release met de geteste .bin worden gemaakt.

Bronnen: [GitHub — handmatig starten](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow), [artifact-upload](https://github.com/actions/upload-artifact), [Espressif Matter-installatie](https://docs.espressif.com/projects/esp-matter/en/release-v1.4.2/esp32c6/developing.html).
