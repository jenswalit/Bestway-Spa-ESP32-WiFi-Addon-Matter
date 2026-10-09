# Home Assistant spa-dashboard

[English](README_EN.md)

Installeer hiermee het spa-dashboard met drie layouts en zeven talen.
De firmware wordt via MQTT verbonden met HA. Matter kan daarnaast actief blijven.

## Bestanden

| Bestand | Plaats in HA |
|---|---|
| `www/bestway-language-card.js` | `www/` naast configuration.yaml |
| `packages/bestway_opwarming_package.yaml` | `packages/` naast configuration.yaml |
| `packages/bestway_gebruik_vandaag_package.yaml` | dezelfde packages-map |
| `layzspa_planner_popup_scripts.yaml` | inhoud toevoegen aan bestaande scripts.yaml |
| `layzspa_dashboard_3_layouts_7_talen.yaml` | volledige ruwe dashboardconfiguratie |

Bij HA OS is die configuratiemap meestal `/config`. Maak geen tweede `/config`-map
of tweede `homeassistant:`-, `packages:`- of `script:`-sleutel aan.

## Benodigd

- Werkende Home Assistant-installatie met MQTT-integratie en een MQTT-broker.
- HACS voor de extra frontendkaarten.
- button-card, layout-card, ApexCharts Card en card-mod.
- Browser Mod als gedownloade component én actieve HA-integratie.
- Buienradar-integratie, met `weather.buienradar`, voor buitentemperatuur en ETA.

Zie pagina 13–18 van de [handleiding](../docs/PtW_Spa_Control_Handleiding.pdf) voor
uitgebreide beginnerstappen. Met MQTT basis `layzspa` verwacht het dashboard onder
meer de entiteiten voor power, filter, heater, bubbels, watertemperatuur,
doeltemperatuur, daadwerkelijk verwarmen en planner. HydroJets worden alleen
getoond wanneer de betreffende entiteit aanwezig is.

## Installeren

1. Kopieer het JavaScript-bestand naar HA `www/`.
2. Bewerk de bestaande dashboardresource tot
   `/local/bestway-language-card.js?v=5`, type **JavaScript-module**. Geen duplicaat.
3. Plaats beide packages en behoud je bestaande packages-instelling. Voor een
   nog ontbrekende inrichting gebruik je binnen de ene homeassistant-sleutel:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

4. Voeg de drie benoemde planner-scripts toe aan je bestaande `scripts.yaml`.
   Deze download heeft geen extra `script:`-kop nodig. Behoud bestaande scripts.
5. Controleer de YAML-configuratie, herlaad Scripts en herstart waar nodig HA.
6. Plaats de complete dashboard-YAML in de ruwe configuratie-editor van een
   dashboard. Het bestand bevat zowel `button_card_templates` als `views`.
7. Herlaad de frontend. Test eerst bediening en planner, daarna grafieken/ETA.

## Planner

Tik op een regel om de popup te openen. De dagenkeuze bepaalt `daysMask`.
**Opdracht actief** schakelt de volledige regel voor de geselecteerde dagen.
De knop **Verwijderen** in de popup verwijdert die regel.

Vasthouden en dubbel tikken hebben in deze dashboardversie geen actie. Gebruik
de popup voor wijzigen en verwijderen, ook als de teksthint anders aangeeft.

De ESP-webplanner kan één dag apart uitschakelen. HA ziet dan
een actieve regel voor de andere dagen en een aparte uitgeschakelde dagregel.
Bij opnieuw aanzetten worden overeenkomstige regels weer samengevoegd.

## Layout en taal

Premium, Spa Resort en Spa Console zijn browserkeuzes. De zeven talen volgen de
HA-taal: Nederlands, Frans, Duits, Engels, Italiaans, Spaans en Pools. Op een telefoon past de plannerhoogte zich aan het aantal zichtbare regels aan,
met de systeemkaart eronder.

## Packages

De opwarmschatting heeft vergelijkbare meetperiodes nodig en is geen gegarandeerde
klaar-tijd. Dagtotalen worden in HA berekend en kunnen afwijken van de ESP-tellers.
Als jouw weerentiteit anders heet, pas de verwijzing in het package bewust aan.
Maak geen dubbele sensoren als de bestaande packages al actief zijn.

## MQTT aansluiten

Zie [de korte MQTT-handleiding](../docs/MQTT_NL.md) voor Mosquitto, de HA-integratie en de instellingen in Spa Control.
