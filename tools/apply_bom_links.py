from pathlib import Path
r=Path(__file__).resolve().parent.parent
for name,heading,paragraph in [
 ('README.md','## Onderdelenlijst','De [Nederlandse ontwerp-BOM](BOM.md), [English BOM](BOM_EN.md) en [CSV](BOM.csv) beschrijven de componentposities, waarden en alternatieve levelshifters. Exacte module- en stekkerkeuzes zijn nog te bevestigen; dit is geen volledig vrijgegeven assemblagebestellijst.'),
 ('README_EN.md','## Bill of materials','The [English design BOM](BOM_EN.md), [Dutch BOM](BOM.md) and [CSV](BOM.csv) list component positions, values and alternative level shifters. Exact module and mating-connector choices remain to be confirmed; this is not a fully released assembly shopping list.')]:
 p=r/'hardware/BWC_V2M'/name
 if not p.exists():print('No existing hardware overview:',name,'— BOM files can still be uploaded.');continue
 t=p.read_text()
 if '](BOM.md)' in t or '](BOM_EN.md)' in t:print('BOM link already present:',name);continue
 t+='\n'+heading+'\n\n'+paragraph+'\n';p.write_text(t);print('Added BOM links:',name)
