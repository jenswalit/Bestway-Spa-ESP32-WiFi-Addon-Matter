// Bestway HA language adapter v2.0.1. Runs in the browser only.
const BWC_TRANSLATIONS = {"nl":{"Niet beschikbaar":"Niet beschikbaar","Aan":"Aan","Uit":"Uit","Vergrendeling":"Vergrendeling","Vergrendeld":"Vergrendeld","Ontgrendeld":"Ontgrendeld","Bubbels":"Bubbels","Filter":"Filter","Heater":"Heater","Power":"Power","Water":"Water","Doel":"Doel","Doeltemperatuur":"Doeltemperatuur","Doeltemperatuur instellen":"Doeltemperatuur instellen","Werkelijk verwarmen":"Werkelijk verwarmen","Heater ingeschakeld":"Heater ingeschakeld","Temperatuur · laatste 24 uur":"Temperatuur · laatste 24 uur","Activiteit · laatste 24 uur":"Activiteit · laatste 24 uur","Buiten · Buienradar":"Buiten · Buienradar","Opwarming · lerende schatting":"Opwarming · lerende schatting","Nieuwe planneropdracht":"Nieuwe planneropdracht","Planneropdracht":"Planneropdracht","Planneropdracht wijzigen":"Planneropdracht wijzigen","Opdracht actief":"Opdracht actief","OPDRACHT":"OPDRACHT","PLANNER":"PLANNER","Actie":"Actie","Dagen":"Dagen","Tijd":"Tijd","Maandag":"Maandag","Dinsdag":"Dinsdag","Woensdag":"Woensdag","Donderdag":"Donderdag","Vrijdag":"Vrijdag","Zaterdag":"Zaterdag","Zondag":"Zondag","Ma":"Ma","Di":"Di","Wo":"Wo","Do":"Do","Vr":"Vr","Za":"Za","Zo":"Zo","Elke dag":"Elke dag","Geen dagen":"Geen dagen","Heater aan":"Heater aan","Heater uit":"Heater uit","Filter aan":"Filter aan","Filter uit":"Filter uit","AirJet aan":"AirJet aan","AirJet uit":"AirJet uit","HydroJets aan":"HydroJets aan","HydroJets uit":"HydroJets uit","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperatuur °C (alleen gebruikt bij Doeltemperatuur)","Verwijderen":"Verwijderen","Opslaan":"Opslaan","Toevoegen":"Toevoegen","Annuleren":"Annuleren","Verwarmt":"Verwarmt","Stand-by":"Stand-by","Onbekend":"Onbekend","Aanwezig":"Aanwezig","Niet aanwezig":"Niet aanwezig","Uitstekend":"Uitstekend","Goed":"Goed","Matig":"Matig","Zwak":"Zwak","IP-adres":"IP-adres","Free heap":"Free heap","Uptime":"Uptime","Model":"Model","Protocol":"Protocol","Filter vervangen":"Filter vervangen","Filter reinigen":"Filter reinigen","Chloor":"Chloor","Geen onderhoudswaarschuwingen":"Geen onderhoudswaarschuwingen","SYSTEEM":"SYSTEEM","Systeem gereed":"Systeem gereed","Verbinding actief, data niet gereed":"Verbinding actief, data niet gereed","Spa-controller niet verbonden":"Spa-controller niet verbonden","GEEN VERBINDING":"GEEN VERBINDING","DATA NIET GEREED":"DATA NIET GEREED","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Temperatuurdata is niet beschikbaar in Home Assistant.","SPA ONLINE":"SPA ONLINE","SPA OFFLINE":"SPA OFFLINE","WACHT OP DATA":"WACHT OP DATA","WATERTEMPERATUUR":"WATERTEMPERATUUR","VERWARMT":"VERWARMT","STAND-BY":"STAND-BY","SPA ACTIEF":"SPA ACTIEF","SPA KLAAR · WATER OP TEMPERATUUR":"SPA KLAAR · WATER OP TEMPERATUUR","SETPOINT":"SETPOINT","● Actief":"● Actief","○ Uitgeschakeld":"○ Uitgeschakeld","Buiten:":"Buiten:","Water / doel:":"Water / doel:","geen weergegevens":"geen weergegevens","Doeltemperatuur bereikt.":"Doeltemperatuur bereikt.","Wacht op geldige spa-gegevens.":"Wacht op geldige spa-gegevens.","Wacht op buitentemperatuur van Buienradar.":"Wacht op buitentemperatuur van Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Schatting gepauzeerd tijdens bubbelen.","Verwarming momenteel niet actief.":"Verwarming momenteel niet actief.","Nog aan het leren:":"Nog aan het leren:","vergelijkbare meetperiodes.":"vergelijkbare meetperiodes.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.","Nog circa":"Nog circa","uur":"uur","minuten.":"minuten.","Verwacht rond":"Verwacht rond","Gemeten tempo:":"Gemeten tempo:","uit":"uit","vergelijkbare periodes.":"vergelijkbare periodes.","Wacht op actieve verwarming en filter.":"Wacht op actieve verwarming en filter.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.","DOELTEMPERATUUR":"DOELTEMPERATUUR","GEEN ACTIEVE FUNCTIES":"GEEN ACTIEVE FUNCTIES","ONDERHOUD":"ONDERHOUD","Spa planning":"Spa planning","Webinterface":"Webinterface","REVISIE":"REVISIE","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen","Activiteit · vandaag":"Activiteit · vandaag","OPWARMEN":"OPWARMEN","TE GAAN":"TE GAAN"},"fr":{"Niet beschikbaar":"Indisponible","Aan":"Activé","Uit":"Désactivé","Vergrendeling":"Verrouillage","Vergrendeld":"Verrouillé","Ontgrendeld":"Déverrouillé","Bubbels":"Bulles","Filter":"Filtre","Heater":"Chauffage","Power":"Alimentation","Water":"Eau","Doel":"Cible","Doeltemperatuur":"Température cible","Doeltemperatuur instellen":"Définir la température cible","Werkelijk verwarmen":"Chauffage effectif","Heater ingeschakeld":"Chauffage activé","Temperatuur · laatste 24 uur":"Température · dernières 24 heures","Activiteit · laatste 24 uur":"Activité · dernières 24 heures","Buiten · Buienradar":"Extérieur · Buienradar","Opwarming · lerende schatting":"Chauffage · estimation adaptative","Nieuwe planneropdracht":"Nouvelle programmation","Planneropdracht":"Commande programmée","Planneropdracht wijzigen":"Modifier la programmation","Opdracht actief":"Commande active","OPDRACHT":"COMMANDE","PLANNER":"PROGRAMMATION","Actie":"Action","Dagen":"Jours","Tijd":"Heure","Maandag":"Lundi","Dinsdag":"Mardi","Woensdag":"Mercredi","Donderdag":"Jeudi","Vrijdag":"Vendredi","Zaterdag":"Samedi","Zondag":"Dimanche","Ma":"Lu","Di":"Ma","Wo":"Me","Do":"Je","Vr":"Ve","Za":"Sa","Zo":"Di","Elke dag":"Tous les jours","Geen dagen":"Aucun jour","Heater aan":"Activer le chauffage","Heater uit":"Désactiver le chauffage","Filter aan":"Activer le filtre","Filter uit":"Désactiver le filtre","AirJet aan":"Activer AirJet","AirJet uit":"Désactiver AirJet","HydroJets aan":"Activer HydroJets","HydroJets uit":"Désactiver HydroJets","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Température °C (uniquement pour la température cible)","Verwijderen":"Supprimer","Opslaan":"Enregistrer","Toevoegen":"Ajouter","Annuleren":"Annuler","Verwarmt":"Chauffe","Stand-by":"En veille","Onbekend":"Inconnu","Aanwezig":"Présent","Niet aanwezig":"Absent","Uitstekend":"Excellent","Goed":"Bon","Matig":"Moyen","Zwak":"Faible","IP-adres":"Adresse IP","Free heap":"Mémoire libre","Uptime":"Durée de fonctionnement","Model":"Modèle","Protocol":"Protocole","Filter vervangen":"Remplacer le filtre","Filter reinigen":"Nettoyer le filtre","Chloor":"Chlore","Geen onderhoudswaarschuwingen":"Aucune alerte de maintenance","SYSTEEM":"SYSTÈME","Systeem gereed":"Système prêt","Verbinding actief, data niet gereed":"Connecté, données pas encore disponibles","Spa-controller niet verbonden":"Contrôleur du spa déconnecté","GEEN VERBINDING":"DÉCONNECTÉ","DATA NIET GEREED":"DONNÉES NON DISPONIBLES","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Pas de connexion au contrôleur. Les données MQTT/HA peuvent être anciennes ou indisponibles.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Le contrôleur est connecté, mais le firmware ne signale pas encore de données valides.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Données de température indisponibles dans Home Assistant.","SPA ONLINE":"SPA CONNECTÉ","SPA OFFLINE":"SPA DÉCONNECTÉ","WACHT OP DATA":"EN ATTENTE DES DONNÉES","WATERTEMPERATUUR":"TEMPÉRATURE DE L’EAU","VERWARMT":"CHAUFFE","STAND-BY":"EN VEILLE","SPA ACTIEF":"SPA ACTIF","SPA KLAAR · WATER OP TEMPERATUUR":"SPA PRÊT · EAU À TEMPÉRATURE","SETPOINT":"CONSIGNE","● Actief":"● Actif","○ Uitgeschakeld":"○ Désactivé","Buiten:":"Extérieur :","Water / doel:":"Eau / cible :","geen weergegevens":"aucune donnée météo","Doeltemperatuur bereikt.":"Température cible atteinte.","Wacht op geldige spa-gegevens.":"En attente de données valides du spa.","Wacht op buitentemperatuur van Buienradar.":"En attente de la température extérieure de Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Estimation suspendue pendant les bulles.","Verwarming momenteel niet actief.":"Chauffage actuellement inactif.","Nog aan het leren:":"Apprentissage en cours :","vergelijkbare meetperiodes.":"périodes de mesure comparables.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"Aucune hausse de température mesurée dans des conditions comparables.","Nog circa":"Encore environ","uur":"h","minuten.":"min.","Verwacht rond":"Prévu vers","Gemeten tempo:":"Vitesse mesurée :","uit":"sur","vergelijkbare periodes.":"périodes comparables.","Wacht op actieve verwarming en filter.":"En attente du chauffage et du filtre actifs.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Estimation avec chauffage continu, sans bulles et dans des conditions comparables. Le vent, la couverture et les changements météo modifient la durée réelle.","DOELTEMPERATUUR":"TEMPÉRATURE CIBLE","GEEN ACTIEVE FUNCTIES":"AUCUNE FONCTION ACTIVE","ONDERHOUD":"MAINTENANCE","Spa planning":"Programmation du spa","Webinterface":"Interface web","REVISIE":"RÉVISION","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Toucher = modifier · Maintenir = activer/désactiver le jour · Double toucher = supprimer","Activiteit · vandaag":"Activité · aujourd’hui","OPWARMEN":"CHAUFFAGE","TE GAAN":"RESTANTS"},"de":{"Niet beschikbaar":"Nicht verfügbar","Aan":"Ein","Uit":"Aus","Vergrendeling":"Sperre","Vergrendeld":"Gesperrt","Ontgrendeld":"Entsperrt","Bubbels":"Sprudel","Filter":"Filter","Heater":"Heizung","Power":"Strom","Water":"Wasser","Doel":"Ziel","Doeltemperatuur":"Zieltemperatur","Doeltemperatuur instellen":"Zieltemperatur einstellen","Werkelijk verwarmen":"Tatsächliches Heizen","Heater ingeschakeld":"Heizung eingeschaltet","Temperatuur · laatste 24 uur":"Temperatur · letzte 24 Stunden","Activiteit · laatste 24 uur":"Aktivität · letzte 24 Stunden","Buiten · Buienradar":"Außen · Buienradar","Opwarming · lerende schatting":"Aufheizen · lernende Schätzung","Nieuwe planneropdracht":"Neuer Zeitplanauftrag","Planneropdracht":"Zeitplanauftrag","Planneropdracht wijzigen":"Zeitplanauftrag bearbeiten","Opdracht actief":"Auftrag aktiv","OPDRACHT":"AUFTRAG","PLANNER":"ZEITPLAN","Actie":"Aktion","Dagen":"Tage","Tijd":"Uhrzeit","Maandag":"Montag","Dinsdag":"Dienstag","Woensdag":"Mittwoch","Donderdag":"Donnerstag","Vrijdag":"Freitag","Zaterdag":"Samstag","Zondag":"Sonntag","Ma":"Mo","Di":"Di","Wo":"Mi","Do":"Do","Vr":"Fr","Za":"Sa","Zo":"So","Elke dag":"Jeden Tag","Geen dagen":"Keine Tage","Heater aan":"Heizung ein","Heater uit":"Heizung aus","Filter aan":"Filter ein","Filter uit":"Filter aus","AirJet aan":"AirJet ein","AirJet uit":"AirJet aus","HydroJets aan":"HydroJets ein","HydroJets uit":"HydroJets aus","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperatur °C (nur für Zieltemperatur)","Verwijderen":"Löschen","Opslaan":"Speichern","Toevoegen":"Hinzufügen","Annuleren":"Abbrechen","Verwarmt":"Heizt","Stand-by":"Bereitschaft","Onbekend":"Unbekannt","Aanwezig":"Vorhanden","Niet aanwezig":"Nicht vorhanden","Uitstekend":"Ausgezeichnet","Goed":"Gut","Matig":"Mittel","Zwak":"Schwach","IP-adres":"IP-Adresse","Free heap":"Freier Heap","Uptime":"Betriebszeit","Model":"Modell","Protocol":"Protokoll","Filter vervangen":"Filter wechseln","Filter reinigen":"Filter reinigen","Chloor":"Chlor","Geen onderhoudswaarschuwingen":"Keine Wartungshinweise","SYSTEEM":"SYSTEM","Systeem gereed":"System bereit","Verbinding actief, data niet gereed":"Verbunden, Daten noch nicht bereit","Spa-controller niet verbonden":"Spa-Controller nicht verbunden","GEEN VERBINDING":"KEINE VERBINDUNG","DATA NIET GEREED":"DATEN NICHT BEREIT","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Keine Controllerverbindung. MQTT/HA-Daten können veraltet oder nicht verfügbar sein.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Controller verbunden, aber die Firmware meldet noch keine gültigen Daten.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Temperaturdaten sind in Home Assistant nicht verfügbar.","SPA ONLINE":"SPA ONLINE","SPA OFFLINE":"SPA OFFLINE","WACHT OP DATA":"WARTE AUF DATEN","WATERTEMPERATUUR":"WASSERTEMPERATUR","VERWARMT":"HEIZT","STAND-BY":"BEREITSCHAFT","SPA ACTIEF":"SPA AKTIV","SPA KLAAR · WATER OP TEMPERATUUR":"SPA BEREIT · WASSER AUF TEMPERATUR","SETPOINT":"SOLLWERT","● Actief":"● Aktiv","○ Uitgeschakeld":"○ Deaktiviert","Buiten:":"Außen:","Water / doel:":"Wasser / Ziel:","geen weergegevens":"keine Wetterdaten","Doeltemperatuur bereikt.":"Zieltemperatur erreicht.","Wacht op geldige spa-gegevens.":"Warte auf gültige Spa-Daten.","Wacht op buitentemperatuur van Buienradar.":"Warte auf Außentemperatur von Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Schätzung während des Sprudelns pausiert.","Verwarming momenteel niet actief.":"Heizung derzeit nicht aktiv.","Nog aan het leren:":"Lernt noch:","vergelijkbare meetperiodes.":"vergleichbare Messperioden.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"Keine positive Erwärmung bei vergleichbaren Bedingungen gemessen.","Nog circa":"Noch etwa","uur":"Std.","minuten.":"Min.","Verwacht rond":"Erwartet gegen","Gemeten tempo:":"Gemessenes Tempo:","uit":"aus","vergelijkbare periodes.":"vergleichbaren Perioden.","Wacht op actieve verwarming en filter.":"Warte auf aktive Heizung und Filter.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Schätzung bei durchgehendem Heizen ohne Sprudel und vergleichbaren Bedingungen. Wind, Abdeckung und Wetteränderungen beeinflussen die tatsächliche Dauer.","DOELTEMPERATUUR":"ZIELTEMPERATUR","GEEN ACTIEVE FUNCTIES":"KEINE AKTIVEN FUNKTIONEN","ONDERHOUD":"WARTUNG","Spa planning":"Spa-Zeitplan","Webinterface":"Weboberfläche","REVISIE":"REVISION","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Tippen = bearbeiten · Halten = Tag ein/aus · Doppeltippen = löschen","Activiteit · vandaag":"Aktivität · heute","OPWARMEN":"AUFHEIZEN","TE GAAN":"VERBLEIBEND"},"en":{"Niet beschikbaar":"Unavailable","Aan":"On","Uit":"Off","Vergrendeling":"Lock","Vergrendeld":"Locked","Ontgrendeld":"Unlocked","Bubbels":"Bubbles","Filter":"Filter","Heater":"Heater","Power":"Power","Water":"Water","Doel":"Target","Doeltemperatuur":"Target temperature","Doeltemperatuur instellen":"Set target temperature","Werkelijk verwarmen":"Actually heating","Heater ingeschakeld":"Heater enabled","Temperatuur · laatste 24 uur":"Temperature · last 24 hours","Activiteit · laatste 24 uur":"Activity · last 24 hours","Buiten · Buienradar":"Outside · Buienradar","Opwarming · lerende schatting":"Warm-up · learned estimate","Nieuwe planneropdracht":"New scheduled command","Planneropdracht":"Scheduled command","Planneropdracht wijzigen":"Edit scheduled command","Opdracht actief":"Command enabled","OPDRACHT":"COMMAND","PLANNER":"SCHEDULE","Actie":"Action","Dagen":"Days","Tijd":"Time","Maandag":"Monday","Dinsdag":"Tuesday","Woensdag":"Wednesday","Donderdag":"Thursday","Vrijdag":"Friday","Zaterdag":"Saturday","Zondag":"Sunday","Ma":"Mon","Di":"Tue","Wo":"Wed","Do":"Thu","Vr":"Fri","Za":"Sat","Zo":"Sun","Elke dag":"Every day","Geen dagen":"No days","Heater aan":"Heater on","Heater uit":"Heater off","Filter aan":"Filter on","Filter uit":"Filter off","AirJet aan":"AirJet on","AirJet uit":"AirJet off","HydroJets aan":"HydroJets on","HydroJets uit":"HydroJets off","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperature °C (only used for target temperature)","Verwijderen":"Delete","Opslaan":"Save","Toevoegen":"Add","Annuleren":"Cancel","Verwarmt":"Heating","Stand-by":"Standby","Onbekend":"Unknown","Aanwezig":"Present","Niet aanwezig":"Not present","Uitstekend":"Excellent","Goed":"Good","Matig":"Fair","Zwak":"Weak","IP-adres":"IP address","Free heap":"Free heap","Uptime":"Uptime","Model":"Model","Protocol":"Protocol","Filter vervangen":"Replace filter","Filter reinigen":"Clean filter","Chloor":"Chlorine","Geen onderhoudswaarschuwingen":"No maintenance warnings","SYSTEEM":"SYSTEM","Systeem gereed":"System ready","Verbinding actief, data niet gereed":"Connected, data not ready","Spa-controller niet verbonden":"Spa controller disconnected","GEEN VERBINDING":"NO CONNECTION","DATA NIET GEREED":"DATA NOT READY","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"No controller connection. MQTT/HA data may be outdated or unavailable.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Controller connected, but firmware has not reported valid data yet.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Temperature data is unavailable in Home Assistant.","SPA ONLINE":"SPA ONLINE","SPA OFFLINE":"SPA OFFLINE","WACHT OP DATA":"WAITING FOR DATA","WATERTEMPERATUUR":"WATER TEMPERATURE","VERWARMT":"HEATING","STAND-BY":"STANDBY","SPA ACTIEF":"SPA ACTIVE","SPA KLAAR · WATER OP TEMPERATUUR":"SPA READY · WATER AT TEMPERATURE","SETPOINT":"SETPOINT","● Actief":"● Enabled","○ Uitgeschakeld":"○ Disabled","Buiten:":"Outside:","Water / doel:":"Water / target:","geen weergegevens":"no weather data","Doeltemperatuur bereikt.":"Target temperature reached.","Wacht op geldige spa-gegevens.":"Waiting for valid spa data.","Wacht op buitentemperatuur van Buienradar.":"Waiting for outside temperature from Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Estimate paused while bubbles are on.","Verwarming momenteel niet actief.":"Heating currently inactive.","Nog aan het leren:":"Still learning:","vergelijkbare meetperiodes.":"comparable measurement periods.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"No positive warming measured in comparable conditions.","Nog circa":"Approximately","uur":"h","minuten.":"min remaining.","Verwacht rond":"Expected around","Gemeten tempo:":"Measured rate:","uit":"from","vergelijkbare periodes.":"comparable periods.","Wacht op actieve verwarming en filter.":"Waiting for active heating and filter.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Estimate assumes continuous heating, no bubbles and comparable conditions. Wind, cover and changing weather affect the actual duration.","DOELTEMPERATUUR":"TARGET TEMPERATURE","GEEN ACTIEVE FUNCTIES":"NO ACTIVE FUNCTIONS","ONDERHOUD":"MAINTENANCE","Spa planning":"Spa schedule","Webinterface":"Web interface","REVISIE":"REVISION","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Tap = edit · Hold = toggle day · Double tap = delete","Activiteit · vandaag":"Activity · today","OPWARMEN":"WARMING UP","TE GAAN":"TO GO"},"it":{"Niet beschikbaar":"Non disponibile","Aan":"Acceso","Uit":"Spento","Vergrendeling":"Blocco","Vergrendeld":"Bloccato","Ontgrendeld":"Sbloccato","Bubbels":"Bolle","Filter":"Filtro","Heater":"Riscaldamento","Power":"Alimentazione","Water":"Acqua","Doel":"Obiettivo","Doeltemperatuur":"Temperatura desiderata","Doeltemperatuur instellen":"Imposta temperatura desiderata","Werkelijk verwarmen":"Riscaldamento effettivo","Heater ingeschakeld":"Riscaldamento abilitato","Temperatuur · laatste 24 uur":"Temperatura · ultime 24 ore","Activiteit · laatste 24 uur":"Attività · ultime 24 ore","Buiten · Buienradar":"Esterno · Buienradar","Opwarming · lerende schatting":"Riscaldamento · stima adattiva","Nieuwe planneropdracht":"Nuovo comando programmato","Planneropdracht":"Comando programmato","Planneropdracht wijzigen":"Modifica comando programmato","Opdracht actief":"Comando attivo","OPDRACHT":"COMANDO","PLANNER":"PROGRAMMA","Actie":"Azione","Dagen":"Giorni","Tijd":"Ora","Maandag":"Lunedì","Dinsdag":"Martedì","Woensdag":"Mercoledì","Donderdag":"Giovedì","Vrijdag":"Venerdì","Zaterdag":"Sabato","Zondag":"Domenica","Ma":"Lun","Di":"Mar","Wo":"Mer","Do":"Gio","Vr":"Ven","Za":"Sab","Zo":"Dom","Elke dag":"Ogni giorno","Geen dagen":"Nessun giorno","Heater aan":"Accendi riscaldamento","Heater uit":"Spegni riscaldamento","Filter aan":"Accendi filtro","Filter uit":"Spegni filtro","AirJet aan":"Accendi AirJet","AirJet uit":"Spegni AirJet","HydroJets aan":"Accendi HydroJets","HydroJets uit":"Spegni HydroJets","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperatura °C (solo per temperatura desiderata)","Verwijderen":"Elimina","Opslaan":"Salva","Toevoegen":"Aggiungi","Annuleren":"Annulla","Verwarmt":"In riscaldamento","Stand-by":"In attesa","Onbekend":"Sconosciuto","Aanwezig":"Presente","Niet aanwezig":"Non presente","Uitstekend":"Ottimo","Goed":"Buono","Matig":"Discreto","Zwak":"Debole","IP-adres":"Indirizzo IP","Free heap":"Memoria libera","Uptime":"Tempo di attività","Model":"Modello","Protocol":"Protocollo","Filter vervangen":"Sostituisci filtro","Filter reinigen":"Pulisci filtro","Chloor":"Cloro","Geen onderhoudswaarschuwingen":"Nessun avviso di manutenzione","SYSTEEM":"SISTEMA","Systeem gereed":"Sistema pronto","Verbinding actief, data niet gereed":"Connesso, dati non pronti","Spa-controller niet verbonden":"Controller spa non connesso","GEEN VERBINDING":"NESSUNA CONNESSIONE","DATA NIET GEREED":"DATI NON PRONTI","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Nessuna connessione al controller. I dati MQTT/HA possono essere obsoleti o non disponibili.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Controller connesso, ma il firmware non segnala ancora dati validi.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Dati di temperatura non disponibili in Home Assistant.","SPA ONLINE":"SPA ONLINE","SPA OFFLINE":"SPA OFFLINE","WACHT OP DATA":"ATTESA DATI","WATERTEMPERATUUR":"TEMPERATURA ACQUA","VERWARMT":"RISCALDAMENTO","STAND-BY":"IN ATTESA","SPA ACTIEF":"SPA ATTIVA","SPA KLAAR · WATER OP TEMPERATUUR":"SPA PRONTA · ACQUA IN TEMPERATURA","SETPOINT":"OBIETTIVO","● Actief":"● Attivo","○ Uitgeschakeld":"○ Disattivato","Buiten:":"Esterno:","Water / doel:":"Acqua / obiettivo:","geen weergegevens":"nessun dato meteo","Doeltemperatuur bereikt.":"Temperatura desiderata raggiunta.","Wacht op geldige spa-gegevens.":"Attesa di dati validi della spa.","Wacht op buitentemperatuur van Buienradar.":"Attesa della temperatura esterna da Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Stima sospesa durante le bolle.","Verwarming momenteel niet actief.":"Riscaldamento attualmente inattivo.","Nog aan het leren:":"Apprendimento in corso:","vergelijkbare meetperiodes.":"periodi di misura comparabili.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"Nessun riscaldamento positivo misurato in condizioni comparabili.","Nog circa":"Ancora circa","uur":"h","minuten.":"min.","Verwacht rond":"Previsto verso","Gemeten tempo:":"Velocità misurata:","uit":"da","vergelijkbare periodes.":"periodi comparabili.","Wacht op actieve verwarming en filter.":"Attesa di riscaldamento e filtro attivi.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Stima con riscaldamento continuo, senza bolle e in condizioni comparabili. Vento, copertura e cambiamenti meteo influenzano la durata effettiva.","DOELTEMPERATUUR":"TEMPERATURA DESIDERATA","GEEN ACTIEVE FUNCTIES":"NESSUNA FUNZIONE ATTIVA","ONDERHOUD":"MANUTENZIONE","Spa planning":"Programmazione spa","Webinterface":"Interfaccia web","REVISIE":"REVISIONE","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Tocca = modifica · Tieni premuto = attiva/disattiva giorno · Doppio tocco = elimina","Activiteit · vandaag":"Attività · oggi","OPWARMEN":"RISCALDAMENTO","TE GAAN":"RIMANENTI"},"es":{"Niet beschikbaar":"No disponible","Aan":"Encendido","Uit":"Apagado","Vergrendeling":"Bloqueo","Vergrendeld":"Bloqueado","Ontgrendeld":"Desbloqueado","Bubbels":"Burbujas","Filter":"Filtro","Heater":"Calefacción","Power":"Encendido","Water":"Agua","Doel":"Objetivo","Doeltemperatuur":"Temperatura objetivo","Doeltemperatuur instellen":"Ajustar temperatura objetivo","Werkelijk verwarmen":"Calentamiento real","Heater ingeschakeld":"Calefacción activada","Temperatuur · laatste 24 uur":"Temperatura · últimas 24 horas","Activiteit · laatste 24 uur":"Actividad · últimas 24 horas","Buiten · Buienradar":"Exterior · Buienradar","Opwarming · lerende schatting":"Calentamiento · estimación adaptativa","Nieuwe planneropdracht":"Nueva orden programada","Planneropdracht":"Orden programada","Planneropdracht wijzigen":"Editar orden programada","Opdracht actief":"Orden activa","OPDRACHT":"ORDEN","PLANNER":"PROGRAMACIÓN","Actie":"Acción","Dagen":"Días","Tijd":"Hora","Maandag":"Lunes","Dinsdag":"Martes","Woensdag":"Miércoles","Donderdag":"Jueves","Vrijdag":"Viernes","Zaterdag":"Sábado","Zondag":"Domingo","Ma":"Lun","Di":"Mar","Wo":"Mié","Do":"Jue","Vr":"Vie","Za":"Sáb","Zo":"Dom","Elke dag":"Todos los días","Geen dagen":"Ningún día","Heater aan":"Encender calefacción","Heater uit":"Apagar calefacción","Filter aan":"Encender filtro","Filter uit":"Apagar filtro","AirJet aan":"Encender AirJet","AirJet uit":"Apagar AirJet","HydroJets aan":"Encender HydroJets","HydroJets uit":"Apagar HydroJets","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperatura °C (solo para temperatura objetivo)","Verwijderen":"Eliminar","Opslaan":"Guardar","Toevoegen":"Añadir","Annuleren":"Cancelar","Verwarmt":"Calentando","Stand-by":"En espera","Onbekend":"Desconocido","Aanwezig":"Presente","Niet aanwezig":"No presente","Uitstekend":"Excelente","Goed":"Buena","Matig":"Regular","Zwak":"Débil","IP-adres":"Dirección IP","Free heap":"Memoria libre","Uptime":"Tiempo de actividad","Model":"Modelo","Protocol":"Protocolo","Filter vervangen":"Cambiar filtro","Filter reinigen":"Limpiar filtro","Chloor":"Cloro","Geen onderhoudswaarschuwingen":"Sin avisos de mantenimiento","SYSTEEM":"SISTEMA","Systeem gereed":"Sistema listo","Verbinding actief, data niet gereed":"Conectado, datos aún no disponibles","Spa-controller niet verbonden":"Controlador del spa desconectado","GEEN VERBINDING":"SIN CONEXIÓN","DATA NIET GEREED":"DATOS NO DISPONIBLES","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Sin conexión al controlador. Los datos MQTT/HA pueden estar desactualizados o no disponibles.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Controlador conectado, pero el firmware aún no indica datos válidos.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Los datos de temperatura no están disponibles en Home Assistant.","SPA ONLINE":"SPA CONECTADO","SPA OFFLINE":"SPA DESCONECTADO","WACHT OP DATA":"ESPERANDO DATOS","WATERTEMPERATUUR":"TEMPERATURA DEL AGUA","VERWARMT":"CALENTANDO","STAND-BY":"EN ESPERA","SPA ACTIEF":"SPA ACTIVO","SPA KLAAR · WATER OP TEMPERATUUR":"SPA LISTO · AGUA A TEMPERATURA","SETPOINT":"OBJETIVO","● Actief":"● Activo","○ Uitgeschakeld":"○ Desactivado","Buiten:":"Exterior:","Water / doel:":"Agua / objetivo:","geen weergegevens":"sin datos meteorológicos","Doeltemperatuur bereikt.":"Temperatura objetivo alcanzada.","Wacht op geldige spa-gegevens.":"Esperando datos válidos del spa.","Wacht op buitentemperatuur van Buienradar.":"Esperando temperatura exterior de Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Estimación pausada durante las burbujas.","Verwarming momenteel niet actief.":"Calefacción actualmente inactiva.","Nog aan het leren:":"Aprendiendo:","vergelijkbare meetperiodes.":"periodos de medición comparables.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"No se ha medido calentamiento positivo en condiciones comparables.","Nog circa":"Quedan aproximadamente","uur":"h","minuten.":"min.","Verwacht rond":"Previsto hacia","Gemeten tempo:":"Ritmo medido:","uit":"a partir de","vergelijkbare periodes.":"periodos comparables.","Wacht op actieve verwarming en filter.":"Esperando calefacción y filtro activos.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Estimación con calefacción continua, sin burbujas y en condiciones comparables. El viento, la cubierta y los cambios meteorológicos afectan al tiempo real.","DOELTEMPERATUUR":"TEMPERATURA OBJETIVO","GEEN ACTIEVE FUNCTIES":"SIN FUNCIONES ACTIVAS","ONDERHOUD":"MANTENIMIENTO","Spa planning":"Programación del spa","Webinterface":"Interfaz web","REVISIE":"REVISIÓN","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Tocar = editar · Mantener = activar/desactivar día · Doble toque = eliminar","Activiteit · vandaag":"Actividad · hoy","OPWARMEN":"CALENTANDO","TE GAAN":"RESTANTES"},"pl":{"Niet beschikbaar":"Niedostępne","Aan":"Włączone","Uit":"Wyłączone","Vergrendeling":"Blokada","Vergrendeld":"Zablokowane","Ontgrendeld":"Odblokowane","Bubbels":"Bąbelki","Filter":"Filtr","Heater":"Grzałka","Power":"Zasilanie","Water":"Woda","Doel":"Cel","Doeltemperatuur":"Temperatura docelowa","Doeltemperatuur instellen":"Ustaw temperaturę docelową","Werkelijk verwarmen":"Rzeczywiste grzanie","Heater ingeschakeld":"Grzałka włączona","Temperatuur · laatste 24 uur":"Temperatura · ostatnie 24 godziny","Activiteit · laatste 24 uur":"Aktywność · ostatnie 24 godziny","Buiten · Buienradar":"Na zewnątrz · Buienradar","Opwarming · lerende schatting":"Nagrzewanie · ucząca się prognoza","Nieuwe planneropdracht":"Nowe zaplanowane polecenie","Planneropdracht":"Zaplanowane polecenie","Planneropdracht wijzigen":"Edytuj zaplanowane polecenie","Opdracht actief":"Polecenie aktywne","OPDRACHT":"POLECENIE","PLANNER":"HARMONOGRAM","Actie":"Działanie","Dagen":"Dni","Tijd":"Godzina","Maandag":"Poniedziałek","Dinsdag":"Wtorek","Woensdag":"Środa","Donderdag":"Czwartek","Vrijdag":"Piątek","Zaterdag":"Sobota","Zondag":"Niedziela","Ma":"Pon","Di":"Wt","Wo":"Śr","Do":"Czw","Vr":"Pt","Za":"Sob","Zo":"Nd","Elke dag":"Codziennie","Geen dagen":"Brak dni","Heater aan":"Włącz grzałkę","Heater uit":"Wyłącz grzałkę","Filter aan":"Włącz filtr","Filter uit":"Wyłącz filtr","AirJet aan":"Włącz AirJet","AirJet uit":"Wyłącz AirJet","HydroJets aan":"Włącz HydroJets","HydroJets uit":"Wyłącz HydroJets","Temperatuur °C (alleen gebruikt bij Doeltemperatuur)":"Temperatura °C (tylko dla temperatury docelowej)","Verwijderen":"Usuń","Opslaan":"Zapisz","Toevoegen":"Dodaj","Annuleren":"Anuluj","Verwarmt":"Grzeje","Stand-by":"Czuwanie","Onbekend":"Nieznane","Aanwezig":"Dostępne","Niet aanwezig":"Niedostępne","Uitstekend":"Doskonały","Goed":"Dobry","Matig":"Średni","Zwak":"Słaby","IP-adres":"Adres IP","Free heap":"Wolna pamięć","Uptime":"Czas działania","Model":"Model","Protocol":"Protokół","Filter vervangen":"Wymień filtr","Filter reinigen":"Wyczyść filtr","Chloor":"Chlor","Geen onderhoudswaarschuwingen":"Brak ostrzeżeń konserwacyjnych","SYSTEEM":"SYSTEM","Systeem gereed":"System gotowy","Verbinding actief, data niet gereed":"Połączono, dane niegotowe","Spa-controller niet verbonden":"Sterownik spa niepołączony","GEEN VERBINDING":"BRAK POŁĄCZENIA","DATA NIET GEREED":"DANE NIEGOTOWE","Geen controllerverbinding. MQTT/HA-data kan hierdoor verouderd of niet beschikbaar zijn.":"Brak połączenia ze sterownikiem. Dane MQTT/HA mogą być nieaktualne lub niedostępne.","Controller is verbonden, maar de firmware meldt nog geen geldige/gereedstaande data.":"Sterownik połączony, ale oprogramowanie nie zgłasza jeszcze prawidłowych danych.","Temperatuurdata is niet beschikbaar in Home Assistant.":"Dane temperatury są niedostępne w Home Assistant.","SPA ONLINE":"SPA ONLINE","SPA OFFLINE":"SPA OFFLINE","WACHT OP DATA":"OCZEKIWANIE NA DANE","WATERTEMPERATUUR":"TEMPERATURA WODY","VERWARMT":"GRZEJE","STAND-BY":"CZUWANIE","SPA ACTIEF":"SPA AKTYWNE","SPA KLAAR · WATER OP TEMPERATUUR":"SPA GOTOWE · WODA NAGRZANA","SETPOINT":"WARTOŚĆ ZADANA","● Actief":"● Aktywne","○ Uitgeschakeld":"○ Wyłączone","Buiten:":"Na zewnątrz:","Water / doel:":"Woda / cel:","geen weergegevens":"brak danych pogodowych","Doeltemperatuur bereikt.":"Osiągnięto temperaturę docelową.","Wacht op geldige spa-gegevens.":"Oczekiwanie na prawidłowe dane spa.","Wacht op buitentemperatuur van Buienradar.":"Oczekiwanie na temperaturę zewnętrzną z Buienradar.","Schatting gepauzeerd tijdens bubbelen.":"Prognoza wstrzymana podczas bąbelków.","Verwarming momenteel niet actief.":"Ogrzewanie obecnie nieaktywne.","Nog aan het leren:":"Trwa nauka:","vergelijkbare meetperiodes.":"porównywalne okresy pomiarowe.","Geen positieve opwarming gemeten bij vergelijkbare omstandigheden.":"Nie zmierzono wzrostu temperatury w porównywalnych warunkach.","Nog circa":"Pozostało około","uur":"godz.","minuten.":"min.","Verwacht rond":"Przewidywany czas","Gemeten tempo:":"Zmierzone tempo:","uit":"z","vergelijkbare periodes.":"porównywalnych okresów.","Wacht op actieve verwarming en filter.":"Oczekiwanie na aktywne ogrzewanie i filtr.","Schatting bij doorverwarmen, zonder bubbels en bij vergelijkbare omstandigheden. Wind, afdekking en veranderend weer beïnvloeden de werkelijke tijd.":"Prognoza zakłada ciągłe grzanie bez bąbelków i porównywalne warunki. Wiatr, pokrywa i zmiany pogody wpływają na rzeczywisty czas.","DOELTEMPERATUUR":"TEMPERATURA DOCELOWA","GEEN ACTIEVE FUNCTIES":"BRAK AKTYWNYCH FUNKCJI","ONDERHOUD":"KONSERWACJA","Spa planning":"Harmonogram spa","Webinterface":"Interfejs WWW","REVISIE":"WERSJA","Tik = wijzigen · Vasthouden = dag aan/uit · Dubbel = verwijderen":"Dotknij = edytuj · Przytrzymaj = przełącz dzień · Dotknij dwukrotnie = usuń","Activiteit · vandaag":"Aktywność · dzisiaj","OPWARMEN":"NAGRZEWANIE","TE GAAN":"DO OSIĄGNIĘCIA"}};
const BWC_KEYS = Object.keys(BWC_TRANSLATIONS.nl).sort((a,b) => b.length-a.length);
const BWC_ESCAPE = s => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const BWC_PATTERN = new RegExp('(?<![\\p{L}\\p{N}])(?:' + BWC_KEYS.map(key => BWC_ESCAPE(key).replace(/\s+/g, '\\s+')).join('|') + ')(?![\\p{L}\\p{N}])', 'gu');
function bwcLanguage(language) {
  const value = String(language || 'nl').toLowerCase().split(/[-_]/)[0];
  return BWC_TRANSLATIONS[value] ? value : 'en';
}
function bwcText(value, language) {
  if (language === 'nl') return value;
  // Preserve inline logo bytes verbatim while translating surrounding text.
  const assets = [];
  value = value.replace(/data:image\/[a-zA-Z0-9.+-]+;base64,[a-zA-Z0-9+/=]+/g, x => {
    assets.push(x); return '__BWC_ASSET_' + (assets.length-1) + '__';
  });
  // Markdown underscores are formatting; preserve technical IDs separately.
  value = value.replace(/\b(?:sensor|binary_sensor|switch|number|script|light|input_boolean|input_number|weather|button|select|automation|climate|cover|fan)\.[A-Za-z0-9_]+\b/g, x => {
    assets.push(x); return '__BWC_ASSET_' + (assets.length-1) + '__';
  });
  return value.replace(BWC_PATTERN, key => BWC_TRANSLATIONS[language][key.replace(/\s+/g,' ')] || key)
    .replace(/__BWC_ASSET_(\d+)__/g, (_,i) => assets[Number(i)]);
}
window.bestwaySpaTranslate = function(value, language) {
  language = bwcLanguage(language);
  if (typeof value === 'string') return bwcThemeText(bwcText(value, language));
  if (Array.isArray(value)) return value.map(v => window.bestwaySpaTranslate(v,language));
  if (value && typeof value === 'object') {
    const result = {};
    for (const [k,v] of Object.entries(value)) result[k] = window.bestwaySpaTranslate(v,language);
    return result;
  }
  return value;
};
function bwcConfig(value, language) {
  if (typeof value === 'string') {
    // Button-card runs JS unchanged; the wrapper translates its result.
    if (/^\s*\[{3,}/.test(value)) return value;
    return bwcThemeText(bwcText(value,language));
  }
  if (Array.isArray(value)) return value.map(v => bwcConfig(v,language));
  if (value && typeof value === 'object') {
    const result = {};
    for (const [k,v] of Object.entries(value)) result[k] = bwcConfig(v,language);
    return result;
  }
  return value;
}
class BestwayLanguageCard extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode:'open'});
    const style = document.createElement('style');
    style.textContent = ':host{display:block;min-width:0;min-height:0}#content{display:block;min-width:0}';
    this._container = document.createElement('div');
    this._container.id = 'content';
    this.shadowRoot.append(style,this._container);
    this._generation = 0;
    this._onLayout = () => this._update();
  }
  connectedCallback() {
    window.addEventListener('bestway-layout-changed',this._onLayout);
    window.addEventListener('storage',this._onLayout);
    this._update();
  }
  disconnectedCallback() {
    window.removeEventListener('bestway-layout-changed',this._onLayout);
    window.removeEventListener('storage',this._onLayout);
  }
  setConfig(config) {
    if (!config.card) throw new Error('Bestway language card requires card configuration.');
    this._config = config;
    this._generation++;
    this._card = null;
    this._buildingLanguage = null;
    this._container.replaceChildren();
    if (this._hass) this._update();
  }
  set hass(value) { this._hass = value; this._update(); }
  _update() {
    if (!this._hass || !this._config) return;
    const language = bwcLanguage(this._hass.locale?.language || this._hass.language);
    const theme = bwcLayout();
    const dark = Boolean(this._hass.themes?.darkMode);
    bwcDark = dark;
    if(theme==='premium'&&dark)document.documentElement?.style.removeProperty('--bestway-view-background');
    else document.documentElement?.style.setProperty('--bestway-view-background',bwcPalette().background);
    const signature = language+'|'+theme+'|'+dark;
    if (this._card && signature === this._language) { this._card.hass = this._hass; return; }
    if (this._buildingLanguage === signature) return;
    this._build(language,theme,signature);
  }
  async _build(language,theme,signature) {
    this._buildingLanguage = signature;
    const generation = ++this._generation;
    try {
      const helpers = await window.loadCardHelpers();
      if (generation !== this._generation) return;
      const card = helpers.createCardElement(bwcConfig(bwcLayoutConfig(this._config.card,theme),language));
      this._container.replaceChildren(card);
      card.hass = this._hass;
      this._card = card;
      this._language = signature;
      this._buildingLanguage = null;
    } catch (error) {
      if (generation !== this._generation) return;
      this._buildingLanguage = null;
      const message = document.createElement('div');
      message.textContent = 'Bestway: ' + error.message;
      this._container.replaceChildren(message);
      console.error('Bestway language card:',error);
    }
  }
  getCardSize() { return this._card?.getCardSize?.() ?? 3; }
  getLayoutOptions() { return this._card?.getLayoutOptions?.() ?? {}; }
}
if (!customElements.get('bestway-language-card')) customElements.define('bestway-language-card',BestwayLanguageCard);

// Layout selection is local to this browser. No HA helpers or ESP traffic.
const BWC_LAYOUT_KEY='bestway-ha-layout-v1';
const BWC_LAYOUTS=['premium','resort','console'];
let bwcDark=false;
function bwcLayout(){try {const v=localStorage.getItem(BWC_LAYOUT_KEY);return BWC_LAYOUTS.includes(v)?v:'premium';}catch{return 'premium';}}
window.bestwaySpaLayout=bwcLayout;
function bwcPalette(){
 if(bwcLayout()==='premium'&&!bwcDark)return {
  surface:'#fffdf8',background:'radial-gradient(circle at 84% 14%, rgba(178,146,88,.12), transparent 38%), radial-gradient(circle at 18% 10%, rgba(36,120,132,.09), transparent 34%), linear-gradient(135deg, #f7f4ed, #edf5f3 58%, #f8f5ee)',
  text:'#173c43',muted:'#526c70',line:'#cbdad5',accent:'#176d7b',active:'#dcefed',gold:'#96631f'
 };

 if(bwcLayout()==='resort')return bwcDark?
  {surface:'#222c31',background:'#171e22',text:'#eff5f2',muted:'#aabbb5',line:'#3c494b',accent:'#8ccebe',active:'#334f47',gold:'#e3b678'}:
  {surface:'#fffdf8',background:'#f4f0e8',text:'#203c3b',muted:'#5c6d65',line:'#dedfd5',accent:'#247065',active:'#d9eee5',gold:'#a56824'};
 return bwcDark?
  {surface:'#1e2b3e',background:'#121c2b',text:'#eff5ff',muted:'#aec0d7',line:'#34465e',accent:'#79c8ff',active:'#263f55',gold:'#f2be82'}:
  {surface:'#f8faff',background:'#e9eef4',text:'#19334e',muted:'#5b6d84',line:'#d0dbe7',accent:'#176da6',active:'#daebf8',gold:'#9c6027'};
}
function bwcRgb(hex){return [1,3,5].map(i=>parseInt(hex.slice(i,i+2),16));}
function bwcThemeText(text){
 if(bwcLayout()==='premium'&&bwcDark)return text;
 const p=bwcPalette();
 const assets=[];
 text=text.replace(/data:image\/[a-zA-Z0-9.+-]+;base64,[a-zA-Z0-9+/=]+/g,x=>{assets.push(x);return '__BWC_THEME_ASSET_'+(assets.length-1)+'__';});
 text=text.replace(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([.\d]+))?\s*\)/g,(all,r,g,b,a)=>{
  r=Number(r);g=Number(g);b=Number(b);
  let color;
  if(r>=150&&g>=150&&b>=150)color=p.text;
  else if(r<80&&g<130&&b<165&&!(r===0&&g===0&&b===0))color=p.surface;
  else if(r<150&&g>=125&&b>=170)color=p.accent;
  if(!color)return all;
  return a===undefined?color:'rgba('+bwcRgb(color).join(',')+','+a+')';
 });
 // Keep small text readable: faded original dark-theme text becomes opaque.
 const textRgb=bwcRgb(p.text);
 text=text.replace(/(^|[^-\w])color\s*:\s*rgba\((\d+),(\d+),(\d+),([.\d]+)\)/g,(all,prefix,r,g,b,a)=>{
  return [Number(r),Number(g),Number(b)].every((v,i)=>v===textRgb[i])?prefix+'color:'+(Number(a)<.8?p.muted:p.text):all;
 });
 const single=text.match(/^rgba\((\d+),(\d+),(\d+),([.\d]+)\)$/);
 if(single&&Number(single[4])>=.4&&[1,2,3].every((i)=>Number(single[i])===textRgb[i-1]))text=Number(single[4])<.8?p.muted:p.text;
 if(bwcLayout()==='premium'){
  // Inherited button-card templates translate their rendered CSS through here.
  // Include white shorthand and the warm/green status accents on pale surfaces.
  const premiumColors={'#ffffff':p.text,'#fff':p.text,'#ffb74d':p.gold,'#ff9a5a':p.gold,'#ffb86b':p.gold,'#86df9a':'#287344','#4de19b':'#237a50','#57d99a':'#237a50','#70d6a5':'#237a50','#65e6df':'#167a78','#ffa34d':p.gold,'#f7b955':p.gold,'#f5bb58':p.gold,'#f2ba59':p.gold,'#ffb457':p.gold,'#ffcc66':p.gold,'#ff8a65':'#b54f2c','#d176ff':'#8750ae','#ff6f61':'#b6433b','#eafffb':p.text,'#f4fffd':p.text,'#b6d4e3':p.muted,'#7e9db6':p.muted,'#7d8b98':p.muted,'#72808d':p.muted};
  text=text.replace(/#[a-fA-F0-9]{6}(?:[a-fA-F0-9]{2})?\b|#[a-fA-F0-9]{3}\b/g,key=>(premiumColors[key.toLowerCase()]||premiumColors[key.slice(0,7).toLowerCase()]||key.slice(0,7))+key.slice(7));
 }
 const colors={'#f4f7fb':p.text,'#f4f8fb':p.text,'#f7f9fc':p.text,'#f4f7fa':p.text,'#63d9f4':p.accent,'#42c5f5':p.accent,'#65a8ff':p.accent,'#57a7ff':p.accent,'#4588ff':p.accent,'#f1b85b':p.gold,'#ff9d4f':p.gold,'#8d9bad':p.muted,'#8d99a8':p.muted};
 text=text.replace(/#[a-fA-F0-9]{6}(?:[a-fA-F0-9]{2})?\b/g,key=>(colors[key.slice(0,7).toLowerCase()]||key.slice(0,7))+key.slice(7));
 if(bwcLayout()==='resort')text=text.replace('clamp(74px,11vw,126px)','clamp(64px,9vw,104px)');
 if(bwcLayout()==='console')text=text.replace('clamp(74px,11vw,126px)','clamp(48px,7vw,78px)');
 return text.replace(/__BWC_THEME_ASSET_(\d+)__/g,(_,i)=>assets[Number(i)]);
}
window.bestwaySpaButtonSurface=(entity,states,variables)=>{
 const on=entity?.state==='on'||states?.[variables?.entity_id]?.state==='on';
 const p=bwcPalette();return on?p.active:p.surface;
};
function bwcStyle(config,section,key,value){
 config.styles??={};config.styles[section]??=[];
 const list=config.styles[section];
 const old=list.find(x=>Object.hasOwn(x,key));if(old)old[key]=value;else list.push({[key]:value});
}
function bwcFieldStyle(config,field,key,value){
 config.styles??={};config.styles.custom_fields??={};
 const list=config.styles.custom_fields[field]??=[];
 const old=list.find(x=>Object.hasOwn(x,key));if(old)old[key]=value;else list.push({[key]:value});
}
function bwcEtaCode(){return `[[[
const mins=Number(states['sensor.bestway_resterende_opwarmtijd']?.state);
const raw=states['sensor.bestway_resterende_opwarmtijd']?.state;
const valid=raw!==undefined&&!['unknown','unavailable','none',''].includes(raw)&&Number.isFinite(mins);
const outside=states['sensor.bestway_buitentemperatuur']?.state;
const p=${JSON.stringify(bwcPalette())};
const heating=states['binary_sensor.bestway_lay_z_spa_daadwerkelijk_verwarmen']?.state==='on';
const bubbles=states['switch.bestway_lay_z_spa_bubbels']?.state==='on';
const message=valid?(mins===0?'Doeltemperatuur bereikt.':('Nog circa '+Math.floor(mins/60)+' uur '+Math.round(mins%60)+' minuten.')):(bubbles?'Schatting gepauzeerd tijdens bubbelen.':(!heating?'Verwarming momenteel niet actief.':'Nog aan het leren:'));
const extra=outside&&!['unknown','unavailable'].includes(outside)?('Buiten: '+outside+' °C'):'';
return window.bestwaySpaTranslate('<div style="padding:12px 14px;border-radius:14px;background:'+p.active+';color:'+p.text+';display:flex;flex-wrap:wrap;gap:8px;justify-content:space-between"><span>'+message+'</span><span>'+extra+'</span></div>',hass.locale?.language||hass.language);
]]]`;}
function bwcUsageCard(){return {
 type:'custom:button-card',template:'spa_surface',show_icon:false,show_name:false,show_state:false,
 triggers_update:['sensor.bestway_verwarming_vandaag','sensor.bestway_filter_vandaag','sensor.bestway_bubbels_vandaag'],
 tap_action:{action:'none'},styles:{grid:[{'grid-template-areas':'"usage"'}],custom_fields:{usage:[{width:'100%'}]}},
 custom_fields:{usage:`[[[
const p=${JSON.stringify(bwcPalette())};
const fields=[['Werkelijk verwarmen','sensor.bestway_verwarming_vandaag'],['Filter','sensor.bestway_filter_vandaag'],['Bubbels','sensor.bestway_bubbels_vandaag']];
const rows=fields.map(([name,id])=>{const raw=states[id]?.state;const h=Number(raw);const valid=raw!==undefined&&!['unknown','unavailable','none',''].includes(raw)&&Number.isFinite(h);const total=valid?Math.round(h*60):0;const value=valid?Math.floor(total/60)+'h '+total%60+'m':'—';return '<div style="display:flex;justify-content:space-between;gap:12px;padding:8px 0"><span>'+name+'</span><span>'+value+'</span></div>';}).join('');
return window.bestwaySpaTranslate('<div style="color:'+p.text+';text-align:left;box-sizing:border-box;padding:0 12px"><div style="font-weight:700;margin-bottom:5px">Activiteit · vandaag</div>'+rows+'</div>',hass.locale?.language||hass.language);
]]]`}
};}
// Premium retains its original geometry, hero effects and all actions.
window.bestwaySpaPremiumSurface=(kind,entity,states,variables)=>{
 const p=bwcPalette();
 const on=entity?.state==='on'||states?.[variables?.entity_id]?.state==='on';
 const accent=variables?.accent||p.accent;
 if(kind==='hero'){
  const heating=states['binary_sensor.bestway_lay_z_spa_daadwerkelijk_verwarmen']?.state==='on';
  const heater=states['switch.bestway_lay_z_spa_heater']?.state==='on';
  const glow=heating?'rgba(194,140,58,.22)':heater?'rgba(194,140,58,.12)':'rgba(38,139,151,.14)';
  return 'radial-gradient(ellipse at 50% 34%, '+glow+', transparent 62%), linear-gradient(145deg, #ffffff, #edf5f2 66%, #f5efe3)';
 }
 if(kind==='control'&&on)return 'linear-gradient(145deg, rgba('+bwcRgb(accent).join(',')+',.22), rgba('+bwcRgb(accent).join(',')+',.08) 65%), linear-gradient(145deg, #ffffff, #f1f7f4)';
 return 'linear-gradient(145deg, #ffffff, '+p.surface+' 62%, #eef5f1)';
};
function bwcPremiumLightConfig(input){
 const cfg=JSON.parse(JSON.stringify(input)),p=bwcPalette();
 function visit(c){
  if(!c||typeof c!=='object')return;
  if(Array.isArray(c)){c.forEach(visit);return;}
  if(c.type==='custom:button-card'){
   const templates=[c.template].flat();
   const control=templates.some(t=>['spa_control','spa_optional_control','spa_micro_button'].includes(t));
   const kind=c.view_layout?.['grid-area']==='hero'?'hero':control?'control':'surface';
   bwcStyle(c,'card','background','[[[ return window.bestwaySpaPremiumSurface('+JSON.stringify(kind)+',entity,states,variables); ]]]');
   bwcStyle(c,'card','color',p.text);
   bwcStyle(c,'card','border',control?'[[[ const on=entity?.state==="on"||states[variables?.entity_id]?.state==="on"; return "1px solid "+(on?(variables?.accent||"'+p.accent+'"):"'+p.line+'"); ]]]':'1px solid '+p.line);
   bwcStyle(c,'card','box-shadow','0 12px 32px #19404217, inset 0 1px 0 #fffefa');
   bwcStyle(c,'name','color',p.text);bwcStyle(c,'label','color',p.muted);
   if(control){
    bwcStyle(c,'icon','color','[[[ const on=entity?.state==="on"||states[variables?.entity_id]?.state==="on"; return on?(variables?.accent||"'+p.accent+'"):"'+p.muted+'"; ]]]');
    bwcStyle(c,'icon','filter','none');
    bwcStyle(c,'img_cell','background','#176d7b12');
    bwcStyle(c,'img_cell','box-shadow','inset 0 1px 0 #fffefa');
   }
  }
  for(const [key,v] of Object.entries(c))if(key!=='styles')visit(v);
 }
 visit(cfg);return cfg;
}
function bwcLayoutConfig(input,theme){
 if(theme==='premium')return bwcDark?input:bwcPremiumLightConfig(input);
 const cfg=JSON.parse(JSON.stringify(input)),p=bwcPalette();
 function visit(c){
  if(!c||typeof c!=='object')return;
  if(Array.isArray(c)){c.forEach(visit);return;}
  if(c.type==='custom:button-card'){
   bwcStyle(c,'card','background','[[[ return window.bestwaySpaButtonSurface(entity,states,variables); ]]]');
   bwcStyle(c,'card','color',p.text);bwcStyle(c,'card','border','1px solid '+p.line);
   bwcStyle(c,'card','border-radius',theme==='resort'?'24px':'16px');
   bwcStyle(c,'card','box-shadow','0 6px 20px rgba(0,0,0,.06)');
   bwcStyle(c,'name','color',p.text);bwcStyle(c,'label','color',p.muted);
   bwcStyle(c,'img_cell','background',p.active);
   bwcStyle(c,'icon','color',p.accent);
  }
  for(const [key,v] of Object.entries(c))if(key!=='styles')visit(v);
 }
 const area=cfg.view_layout?.['grid-area'];
 if(area==='hero'){
  delete cfg.custom_fields.waves;delete cfg.custom_fields.ambient;
  cfg.custom_fields.eta=bwcEtaCode();
  cfg.triggers_update=[...(cfg.triggers_update||[]),'sensor.bestway_buitentemperatuur','sensor.bestway_resterende_opwarmtijd'];
  bwcStyle(cfg,'card','min-height',theme==='resort'?'330px':'245px');
  bwcStyle(cfg,'card','padding',theme==='resort'?'24px':'18px');
  bwcStyle(cfg,'grid','grid-template-areas',theme==='resort'?'"eyebrow" "temperature" "mode" "target" "controls" "statusline" "eta"':'"eyebrow target" "temperature target" "mode mode" "controls controls" "statusline statusline" "eta eta"');
  bwcStyle(cfg,'grid','grid-template-columns',theme==='resort'?'1fr':'minmax(0,1fr) auto');
  bwcStyle(cfg,'grid','grid-template-rows','auto');
  bwcStyle(cfg,'grid','row-gap','12px');
  bwcFieldStyle(cfg,'eta','width','100%');
  if(theme==='console')for(const field of ['eyebrow','temperature'])bwcFieldStyle(cfg,field,'justify-self','start');
 }
 if(area==='controls'){
  cfg.layout['grid-gap']=theme==='resort'?'10px':'8px';
  cfg.layout['grid-template-columns']=theme==='resort'?'repeat(5,minmax(0,1fr))':'repeat(3,minmax(0,1fr))';
  cfg.layout.mediaquery['(max-width: 640px)']['grid-template-columns']='repeat(2,minmax(0,1fr))';
  for(const card of cfg.cards){
   bwcStyle(card,'card','min-height',theme==='resort'?'108px':'82px');
   bwcStyle(card,'card','padding',theme==='resort'?'14px':'12px');
   if(theme==='console'){
    bwcStyle(card,'grid','grid-template-areas','"i n" "i l"');
    bwcStyle(card,'grid','grid-template-columns','38px minmax(0,1fr)');
    bwcStyle(card,'grid','grid-template-rows','min-content min-content');
    bwcStyle(card,'img_cell','width','34px');bwcStyle(card,'img_cell','height','34px');
    bwcStyle(card,'name','justify-self','start');bwcStyle(card,'label','justify-self','start');
   }
  }
 }
 if(area==='history'&&theme==='console')cfg.card.cards.unshift(bwcUsageCard());
 if(area==='planner')cfg.layout['grid-gap']='8px';
 if(area==='header')bwcStyle(cfg,'card','padding',theme==='resort'?'20px':'16px');
 visit(cfg);return cfg;
}
class BestwayLayoutPicker extends HTMLElement {
 constructor(){super();this.attachShadow({mode:'open'});this._onChange=()=>this._render();}
 setConfig(){this._render();}
 set hass(value){this._hass=value;this._render();}
 connectedCallback(){window.addEventListener('bestway-layout-changed',this._onChange);window.addEventListener('storage',this._onChange);this._render();}
 disconnectedCallback(){window.removeEventListener('bestway-layout-changed',this._onChange);window.removeEventListener('storage',this._onChange);}
 _render(){
  const current=bwcLayout(),dark=Boolean(this._hass?.themes?.darkMode);
  const text=dark?'#eef3f7':'#173443',bg=dark?'#1e2b35':'#f4f7f9',line=dark?'#405361':'#cad5dd';
  this.shadowRoot.innerHTML='<style>:host{display:block}nav{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:10px 12px;background:'+bg+';color:'+text+';border:1px solid '+line+';border-radius:16px;font:14px system-ui}button{font:inherit;min-height:44px;padding:8px 14px;border:1px solid '+line+';border-radius:12px;background:transparent;color:inherit}button[aria-pressed=true]{background:'+text+';color:'+bg+'}@media(max-width:420px){nav{gap:6px;padding:8px}button{flex:1;padding:8px;font-size:12px}}</style>';
  const nav=document.createElement('nav');nav.setAttribute('aria-label','Spa layout');
  for(const [id,name] of [['premium','Premium'],['resort','Spa Resort'],['console','Spa Console']]){
   const b=document.createElement('button');b.type='button';b.textContent=name;b.setAttribute('aria-pressed',String(current===id));
   b.addEventListener('click',()=>{try{localStorage.setItem(BWC_LAYOUT_KEY,id);}catch{}window.dispatchEvent(new CustomEvent('bestway-layout-changed'));});nav.append(b);
  }
  this.shadowRoot.append(nav);
 }
 getCardSize(){return 1;}
}
if(!customElements.get('bestway-layout-picker'))customElements.define('bestway-layout-picker',BestwayLayoutPicker);
