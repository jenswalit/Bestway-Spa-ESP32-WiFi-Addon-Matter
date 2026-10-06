# Spa Control met Home Assistant verbinden via MQTT

**Nederlands** | [English](MQTT_EN.md)

Deze korte uitleg gaat uit van Home Assistant OS en een Spa Control-module die al met je thuis-WiFi verbonden is.

## 1. Broker installeren

Ga in Home Assistant naar **Instellingen → Apps → App installeren**. In oudere versies heet dit **Add-ons → Add-onwinkel**. Installeer **Mosquitto broker**, schakel starten bij opstarten en Watchdog in en start de app. Heb je al een werkende broker? Behoud die en sla deze stap over.

## 2. MQTT-gebruiker maken

Maak onder **Instellingen → Personen → Gebruikers** een gebruiker `spacontrol` met een eigen wachtwoord. Beheerdersrechten zijn niet nodig. Is Gebruikers niet zichtbaar? Schakel Geavanceerde modus in je profiel in. Deze gegevens gebruik je in Spa Control.

## 3. MQTT-integratie activeren

Ga naar **Instellingen → Apparaten & diensten** en voeg de ontdekte **MQTT**-integratie toe. Verschijnt die niet, voeg MQTT handmatig toe met het lokale broker-IP, poort `1883` en de gemaakte gebruikersgegevens. Houd MQTT discovery ingeschakeld. Voeg geen tweede integratie toe als MQTT al werkt.

## 4. Spa Control instellen

Open de webinterface en ga naar **Instellingen → MQTT-instellingen**.

| Instelling | Waarde |
|---|---|
| MQTT inschakelen | Aan |
| Broker of IP-adres | Lokaal IP van Home Assistant, bijvoorbeeld `192.168.1.10` |
| Poort | `1883` |
| Gebruikersnaam | `spacontrol` |
| Wachtwoord | Je wachtwoord uit stap 2 |
| Client-ID | `LayZSpaController` |
| Basistopic | `layzspa` |
| Home Assistant Discovery | Aan |

Vul alleen het brokeradres in, zonder `http://` of `:8123`. Draait je broker ergens anders, gebruik dan dat adres. Laat publicatietopics op hun standaardwaarden staan en sla op. Geef bij meerdere modules iedere module een eigen client-ID en basistopic; het meegeleverde dashboard gaat standaard uit van één module met `layzspa`.

## 5. Controleren

Spa Control hoort MQTT als online te tonen. Zoek in Home Assistant onder **Instellingen → Apparaten & diensten → MQTT → Apparaten** naar **Bestway Lay-Z-Spa**. Controleer de watertemperatuur en probeer bijvoorbeeld de filterknop.

Voor deze verbinding zijn Matter en HACS niet nodig. Installeer het [uitgebreide dashboard](../home-assistant/README.md) afzonderlijk.

Offline? Controleer broker-IP, gebruikersnaam, wachtwoord en of Mosquitto draait. Wel online maar geen apparaat? Controleer discovery in Spa Control en Home Assistant.

Officiële uitleg: [Mosquitto instellen](https://github.com/home-assistant/addons/blob/master/mosquitto/DOCS.md) en [Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt/).
