set(WEB_ASSETS
    web/index.html
    web/css/app.css
    web/js/app.js
    web/lang/languages.json
    web/lang/nl.json
    web/lang/en.json
    web/lang/de.json
    web/lang/fr.json
    web/pages/dashboard.html
    web/pages/mqtt.html
    web/pages/settings.html
    web/pages/spa.html
    web/pages/system.html
    web/pages/wifi.html
    web/manifest.json
    web/sw.js
    web/icons/icon.svg
    web/icons/favicon-32.png
    web/icons/icon-64.png
    web/icons/icon-192.png
    web/icons/icon-512.png
    web/icons/apple-touch-icon.png
    web/icons/ptw-logo.png
    web/icons/matter-qr.png
)

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_wifi esp_netif esp_system spa_core bestway_bus scheduler mqtt_manager status_led
    EMBED_FILES ${WEB_ASSETS}
    PRIV_REQUIRES nvs_flash app_update
)
