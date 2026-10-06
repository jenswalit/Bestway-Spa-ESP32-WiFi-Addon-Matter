# Hardware en aansluiting

Geteste opstelling: ESP32-C6-DevKitC-1-N8, MIAMI2021, 6-wire Type1.

| Verbinding | Signaal | GPIO |
|---|---|---:|
| CIO — pompzijde | DATA | 23 |
| CIO — pompzijde | CLK | 22 |
| CIO — pompzijde | CS | 21 |
| DSP — fysiek display | DATA | 20 |
| DSP — fysiek display | CLK | 19 |
| DSP — fysiek display | CS | 18 |
| Audio | LEDC-uitgang | 11 |
| Temperatuurreserve | Reserve | 0 |
| RGB-statusled | LED | 8 |

## Module plaatsen

1. Spa uitschakelen en netstekker uit het stopcontact halen.
2. Originele displaystekker uit de pompprint halen.
3. Die displaystekker verbinden met de DSP-zijde van de module.
4. CIO-zijde verbinden met de vrijgekomen connector op de pompprint.
5. Module droog en beschermd monteren; kabels en sluiting controleren.
6. Pomp sluiten, inschakelen en daarna WiFi instellen.

Foto's en de gebruikersstappen staan op pagina 37–39 van de
[handleiding](PtW_Spa_Control_Handleiding.pdf). Laat netspanningsaansluitingen,
aardverbindingen en vermogenskabels ongemoeid.

