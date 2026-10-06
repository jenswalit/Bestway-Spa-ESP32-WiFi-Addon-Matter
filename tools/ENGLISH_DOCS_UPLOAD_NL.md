# Engelse documentatie toevoegen aan GitHub

Dit pakket voegt Engelse build-, Matter-, hardware-, MQTT-, Home Assistant-, firmware-, bijdrage- en herkomstpagina's toe. Het script past links aan in bestaande pagina's en behoudt hun overige inhoud. Het vervangt geen firmware, configuratie, dashboard of LICENSE.

Ga in de terminal naar jouw bestaande lokale GitHub-repository. Controleer eerst `git remote -v`. Stop bij een fout in een van de onderstaande stappen.

```bash
git pull --ff-only
unzip -o ~/Downloads/Bestway_GitHub_English_Documentation.zip -d .
python3 tools/apply_english_docs.py
git diff --stat
git status --short
```

Controleer de wijzigingen. Voeg alleen de documentatie en het bijbehorende script toe:

```bash
git add -- README.md README_EN.md CONTRIBUTING.md CONTRIBUTING_EN.md CHANGELOG.md CHANGELOG_EN.md docs/BUILD_NL.md docs/BUILD_EN.md docs/MATTER_NL.md docs/MATTER_EN.md docs/HARDWARE_NL.md docs/HARDWARE_EN.md docs/HERKOMST_EN_LICENTIES.md docs/ORIGIN_AND_LICENSES_EN.md docs/MQTT_NL.md docs/MQTT_EN.md home-assistant/README.md home-assistant/README_EN.md firmware/README.md firmware/README_EN.md tools/apply_english_docs.py tools/ENGLISH_DOCS_UPLOAD_NL.md
git diff --cached --stat
git commit -m "Add English guides and bilingual documentation links"
git push
```

Het script kan opnieuw worden uitgevoerd zonder dubbele taallinks toe te voegen. Onbekende tekst wordt niet gewist. Als jouw bestaande documentatie afwijkt van de eerder gemaakte projectkopie, controleer dan de Engelse inhoud en resterende verwijzingen. Upload het V2M-hardwarepakket afzonderlijk als het nog niet in GitHub staat; de hardwarelinks verwijzen daarnaar.
