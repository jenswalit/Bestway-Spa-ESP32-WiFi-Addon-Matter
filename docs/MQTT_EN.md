# Connecting Spa Control to Home Assistant through MQTT

**English** | [Nederlands](MQTT_NL.md)

This short guide assumes Home Assistant OS and a Spa Control module already connected to your home WiFi.

## 1. Install the broker

In Home Assistant, open **Settings → Apps → Install app**. Older versions call this **Add-ons → Add-on Store**. Install **Mosquitto broker**, enable start on boot and Watchdog, and start it. If you already use a working broker, keep it and skip this step.

## 2. Create an MQTT user

Under **Settings → People → Users**, create a user named `spacontrol` with your own password. Administrator privileges are not required. If Users is hidden, enable Advanced Mode in your profile. Use these credentials in Spa Control.

## 3. Enable the MQTT integration

Open **Settings → Devices & services** and add the discovered **MQTT** integration. If it is not discovered, add MQTT manually using your broker's local IP address, port `1883` and the credentials above. Keep MQTT discovery enabled. Do not add a second integration if MQTT already works.

## 4. Configure Spa Control

Open the module's web interface, then **Instellingen → MQTT-instellingen** (Settings → MQTT settings).

| Setting | Value |
|---|---|
| Enable MQTT | On |
| Broker or IP address | Home Assistant's local IP, for example `192.168.1.10` |
| Port | `1883` |
| Username | `spacontrol` |
| Password | Your password from step 2 |
| Client ID | `LayZSpaController` |
| Base topic | `layzspa` |
| Home Assistant Discovery | On |

Enter only the broker address, without `http://` or `:8123`. If the broker runs elsewhere, use that broker's address instead. Leave publication topics at their defaults and save. With multiple modules, give each a different client ID and base topic; the supplied dashboard defaults target a single module using `layzspa`.

## 5. Check the connection

Spa Control should show MQTT as online. In Home Assistant, open **Settings → Devices & services → MQTT → Devices** and find **Bestway Lay-Z-Spa**. Check water temperature and test a control such as the filter.

This connection does not require Matter or HACS. Install the [custom dashboard](../home-assistant/README_EN.md) separately.

If offline, check broker address, username, password and whether Mosquitto is running. If online but no device appears, check discovery in Spa Control and Home Assistant.

Official references: [Mosquitto setup](https://github.com/home-assistant/addons/blob/master/mosquitto/DOCS.md) and [Home Assistant MQTT integration](https://www.home-assistant.io/integrations/mqtt/).
