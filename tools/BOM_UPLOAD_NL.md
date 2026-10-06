# Ontwerp-BOM publiceren

Download Bestway_GitHub_BOM_V2M.zip naar Downloads. Ga in de terminal naar de bestaande lokale GitHub-repository. Stop bij een fout.

```bash
git pull --ff-only
unzip -o ~/Downloads/Bestway_GitHub_BOM_V2M.zip -d .
python3 tools/apply_bom_links.py
git diff --stat
git status --short
```

Controleer de wijzigingen en publiceer:

```bash
git add -- hardware/BWC_V2M tools/apply_bom_links.py tools/BOM_UPLOAD_NL.md
git diff --cached --stat
git commit -m "Add bilingual V2M design BOM and component CSV files"
git push
```

De BOM is op componentniveau volledig voor de bronprint met 27 componentposities. U2/U3 zijn alternatieven. De exacte levelshiftermodule en werkelijk te plaatsen spa-connectoren moeten nog bevestigd worden. De nummers bij U3 zijn chipgegevens en geen geschikt bestelnummer voor de module. Een complete automatische assemblagebestelling kan niet met alleen deze bestanden worden geplaatst.
