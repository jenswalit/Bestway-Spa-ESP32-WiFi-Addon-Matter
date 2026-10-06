# Het project netjes op GitHub zetten

Doelrepository:
https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter

Je zet de uitgepakte bestanden op GitHub, niet alleen dit ZIP-bestand. De root moet
README.md, firmware, home-assistant en docs bevatten. Je bestaande werkmap
`~/projects/Bestway_Matter_v5` kan gewoon blijven staan.

## 1 Downloaden en uitpakken

Download `Bestway_GitHub_Klaar.zip` en bewaar het in `~/Downloads`. De uitgepakte map
heet `Bestway-Spa-ESP32-WiFi-Addon-Matter`. Op jouw Ubuntu-computer:

```bash
sudo apt update
sudo apt install git gh unzip
unzip -n ~/Downloads/Bestway_GitHub_Klaar.zip -d ~/Downloads
```

Staat je download elders, pas alleen dat pad aan. Dit pakket bevat geen `.git`-map;
je eigen GitHub-geschiedenis wordt straks door clone opgehaald.

## 2 Bij GitHub aanmelden

```bash
gh auth login --hostname github.com --git-protocol https --web
gh auth setup-git
gh auth status
```

Volg de code en browserinstructies. Meld je aan met **lowlander10000** en geef de
CLI toegang tot je repository. Gebruik de browseraanmelding; plak geen token in
deze chat of in een bestand van het project. De CLI-aanmelding op jouw computer
staat los van de GitHub-koppeling in ChatGPT.

## 3 Repository lokaal ophalen

```bash
mkdir -p ~/projects
git clone https://github.com/lowlander10000/Bestway-Spa-ESP32-WiFi-Addon-Matter.git ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
```

Bij een lege repository kan Git melden dat je een lege repository hebt gekloond.
Dat is normaal. Bestaat deze lokale map al, gebruik hem alleen als het echt jouw
checkout van deze repository is; controleer `git remote -v` en `git status`.
Een fout bij clone is geen reden om force-push te gebruiken.

## 4 Pakket erin zetten

```bash
cp -a ~/Downloads/Bestway-Spa-ESP32-WiFi-Addon-Matter/. ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter/
cd ~/projects/Bestway-Spa-ESP32-WiFi-Addon-Matter
git status --short
```

Dit neemt ook `.gitignore` en `.gitattributes` mee. Er zit geen `.git`-map in het
pakket die je bestaande geschiedenis zou vervangen. Een eventueel bestaande
README wordt bijgewerkt; eigen andere bestanden worden niet verwijderd.

## 5 Naam voor de commit instellen

Alleen nodig als Git jouw naam/e-mail nog niet kent. Deze instelling geldt alleen
voor deze repository:

```bash
git config user.name "Perry"
git config user.email "62474281+lowlander10000@users.noreply.github.com"
```

Dit gebruikt het numerieke account-ID en de login van het gecontroleerde account.
Je kunt in GitHub Settings > Emails de eigen noreply-adresvariant controleren en
zo nodig aanpassen.

## 6 Controleren en vastleggen

```bash
git add .
git diff --cached --stat
git status --short
git commit -m "Add confirmed ESP32-C6 Matter firmware, HA dashboard and manual"
```

Controleer de lijst vóór de commit. Geen persoonlijke backup-JSON, lograpport,
flashdump of WiFi/MQTT/Matter-codebestand toevoegen. De `.gitignore` helpt daarbij.
De eerste commit zet geen firmware op je ESP; dit is alleen versiebeheer.

## 7 Uploaden

```bash
git push -u origin HEAD
```

`HEAD` betekent de huidige branch. Dit bewaart de branchekeuze en eventuele
bestaande GitHub-geschiedenis. Gebruik geen `--force`. Bij een geweigerde push
vanwege nieuwe commits op GitHub: stop, bekijk de fout en synchroniseer eerst.

Open daarna de repositorylink. Je moet README.md direct zien en de mappen firmware,
home-assistant, docs, tests en tools. Is de repository leeg begonnen en zie je de
bestanden op een andere branch, stel die branch in GitHub als standaardbranch in.

## Later een wijziging bijwerken

Vanuit de checkout:

```bash
git status
git pull --ff-only
git switch -c fix/korte-omschrijving
```

Verander bestanden in `firmware/`, bouw en test. Commit alleen relevante bestanden:

```bash
git add firmware/
git diff --cached --stat
git commit -m "Describe the tested change"
git push -u origin HEAD
```

Open op GitHub een pull request naar je standaardbranch. Beschrijf wat veranderd
is en hoe het is getest. Houd jouw werkende oude projectmap voorlopig als backup.

## Release maken

Maak een release pas van een werkelijk gebouwde en geteste commit. Je kunt als
basistag `stable-2026-10-06` gebruiken. Voeg de bij die commit gebouwde app `.bin`
en release-opmerkingen toe. GitHub maakt broncode-ZIP's van een tag automatisch.
Een privérepository blijft privé; dit pakket verandert de zichtbaarheid niet.

## Officiële uitleg

- https://cli.github.com/manual/gh_auth_login
- https://cli.github.com/manual/gh_auth_setup-git
- https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github
