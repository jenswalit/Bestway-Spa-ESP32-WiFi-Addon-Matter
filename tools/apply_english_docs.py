"""Add language links to existing Markdown without replacing whole pages."""
from pathlib import Path
r=Path(__file__).resolve().parent.parent
if not (r/'README.md').is_file():
 raise SystemExit('Run this script inside the extracted GitHub repository, not in the update ZIP directory.')
updated=[]
def edit(name,fn):
 p=r/name
 if not p.exists():print('Skipped missing existing page:',name);return
 before=p.read_text(encoding='utf8');after=fn(before)
 if before!=after:p.write_text(after,encoding='utf8');updated.append(name)
def language_link(text,target):
 if f']({target})' in text:return text
 lines=text.splitlines(keepends=True)
 index=next((i for i,s in enumerate(lines) if s.startswith('# ')),None)
 if index is None:return text
 lines.insert(index+1,f'\n[English]({target})\n')
 return ''.join(lines)
for nl,en in [('docs/BUILD_NL.md','BUILD_EN.md'),('docs/MATTER_NL.md','MATTER_EN.md'),('docs/HARDWARE_NL.md','HARDWARE_EN.md'),('docs/HERKOMST_EN_LICENTIES.md','ORIGIN_AND_LICENSES_EN.md'),('home-assistant/README.md','README_EN.md'),('firmware/README.md','README_EN.md'),('CONTRIBUTING.md','CONTRIBUTING_EN.md'),('CHANGELOG.md','CHANGELOG_EN.md')]:
 edit(nl,lambda t,en=en:language_link(t,en))
def english_readme(t):
 for a,b in [('docs/BUILD_NL.md','docs/BUILD_EN.md'),('docs/MATTER_NL.md','docs/MATTER_EN.md'),('docs/HARDWARE_NL.md','docs/HARDWARE_EN.md'),('docs/HERKOMST_EN_LICENTIES.md','docs/ORIGIN_AND_LICENSES_EN.md'),('home-assistant/README.md','home-assistant/README_EN.md'),('CONTRIBUTING.md','CONTRIBUTING_EN.md'),('CHANGELOG.md','CHANGELOG_EN.md')]:
  t=t.replace(f']({a})',f']({b})')
 t=t.replace('The user manual is available in English and Dutch. The other linked guides are currently in Dutch.','The user manual and the guides below are available in English and Dutch. Each guide links to its other language.')
 t=t.replace('[licensing information (Dutch)]','[origin and licensing information]')
 t=t.replace('Read the hardware information. Schematics, Gerber files and a complete parts list will be added later.','Read the hardware information and the V2M hardware package. Check its verification status before ordering; the complete assembly parts list is still being finalized.')
 if '](docs/MQTT_EN.md)' not in t:
  t=t.replace('| Install the Home Assistant dashboard |','| Connect Spa Control through MQTT | [MQTT setup](docs/MQTT_EN.md) |\n| Install the Home Assistant dashboard |')
 return t
edit('README_EN.md',english_readme)
def dutch_readme(t):
 t=t.replace('Bekijk de hardware-informatie. Schema, Gerber-bestanden en een complete onderdelenlijst worden later toegevoegd.','Bekijk de hardware-informatie en het V2M-hardwarepakket. Controleer de verificatiestatus vóór bestellen; de complete assemblage-onderdelenlijst wordt nog afgerond.')
 if '](docs/MQTT_NL.md)' not in t:
  t=t.replace('| Home Assistant-dashboard installeren |','| Spa Control via MQTT koppelen | [MQTT instellen](docs/MQTT_NL.md) / [English](docs/MQTT_EN.md) |\n| Home Assistant-dashboard installeren |')
 return t
edit('README.md',dutch_readme)
def origin(t):
 return t.replace('Schema, Gerber-bestanden en een complete onderdelenlijst zijn nog niet beschikbaar.\nDe licentievoorwaarden voor die toekomstige bestanden worden bij publicatie vermeld.','Het V2M-hardwarepakket bevat schema, PCB-bron en Gerber-bestanden. Zie [het hardwareoverzicht](../hardware/BWC_V2M/README.md) nadat dat pakket is geüpload. Een complete assemblage-onderdelenlijst wordt nog afgerond. Een afzonderlijke hardwarelicentie is nog niet vastgesteld; de softwarelicentie wordt hier niet automatisch uitgebreid tot de PCB-ontwerpbestanden.')
edit('docs/HERKOMST_EN_LICENTIES.md',origin)
def hardware(t):
 if '../hardware/BWC_V2M/README.md' in t:return t
 return t+'\n## PCB-ontwerp en bestelbestanden\n\nZie [het V2M-hardwarepakket](../hardware/BWC_V2M/README.md) voor schema, PCB-bron, Gerber en de actuele verificatiestatus, nadat dat pakket is geüpload. Een volledige assemblage-onderdelenlijst en montagehandleiding worden nog afgerond.\n'
edit('docs/HARDWARE_NL.md',hardware)
def ha(t):
 if '../docs/MQTT_NL.md' in t:return t
 return t+'\n## MQTT aansluiten\n\nZie [de korte MQTT-handleiding](../docs/MQTT_NL.md) voor Mosquitto, de HA-integratie en de instellingen in Spa Control.\n'
edit('home-assistant/README.md',ha)
print('Updated language/navigation links in',len(updated),'existing pages:')
for name in updated:print(' ',name)
print('Documentation only; firmware sources, dashboard YAML and LICENSE are unchanged.')
