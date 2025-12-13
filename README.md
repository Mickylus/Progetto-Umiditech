# Umiditech — Monitoraggio Umidità e Temperatura con ESP32

## Descrizione progetto

Umiditech è un sistema basato su ESP32 per il monitoraggio di umidità e temperatura tramite sensore DHT11.  
Il dispositivo:

- Visualizza i valori su display LCD I2C.
- Registra storico su scheda SD (`storico.json`).
- Fornisce un’interfaccia web accessibile tramite WiFi (rete normale o Access Point).
- Permette configurazione di WiFi, intervalli di aggiornamento e intervallo di salvataggio dei grafici tramite Web UI.

---

## Pinout

| Componente  | Pin ESP32                   |
|------------ |---------------------------- |
| SD Card (SPI) | MOSI = 23, MISO = 19, SCK = 18, CS = 5 |
| LCD I2C       | SDA = 21, SCL = 22         |
| DHT11         | DHT Pin = 13, Vcc = 5V, GND = GND |
| Potenziometro | Analog In = 34             |
| Buzzer        | 14                         |
| LED           | 15                         |

---

## Codice / Funzioni principali

**Firmware:** `src/main.cpp`  

### Setup e inizializzazione

- `setup()`:
  - Inizializza Serial, I2C (LCD), SPI (SD), DHT11, NTP client.
  - Monta SD e avvia server HTTP asincrono.
  - Imposta orario: NTP se internet disponibile, altrimenti orario di compilazione (`setCompileTime()`).

### Loop principale

- Aggiornamento dati sensori ogni `REFRESH_RATE`.
- Salvataggio storico su SD ogni `GRAPH_RATE`.
- Aggiornamento display LCD.
- Controllo allarme umidità >65% (LED + buzzer).
- Gestione errori DHT11 e SD.

### Gestione dati

- `aggiornaDati()` → legge DHT11 e potenziometro, costruisce JSON per `/dati.json`.
- `writeFile(path, data)` → scrive JSON su SD, cancellando il file precedente.
- `salvaStorico()` / `generaStorico()` → append dei dati su `/storico.json`.
- `caricaConfig()` → legge `/settings.json` e aggiorna refresh rate, intervallo grafico e `MAX_MEM`.

### Gestione WiFi

- `connectWiFi()`:
  - Connessione a rete salvata o default.
  - Se fallisce, avvia AP (`ESP32-Umiditech`).
  - `isOpenWifi` indica se rete con internet (per NTP).
- `WiFi.localIP()` o `WiFi.softAPIP()` mostrano IP corretto sul display.

### Server HTTP

- `GET /settings.json` → restituisce JSON di configurazione.
- `POST /settings.json` → salva nuovo JSON e aggiorna configurazione.
- `onNotFound` → serve i file presenti sulla SD (HTML, CSS, JS, immagini, font).

---

## Controlli e funzionamento sito / WiFi

### Web UI

- **Pagina principale (`index.html`)**
  - Mostra valori umidità, temperatura e volume.
  - Grafici storico (`grafico_humidity`, `grafico_temperature`) tramite `storico.json`.
- **Pagina impostazioni (`settings.html`)**
  - Configurazione WiFi, refresh rate e intervallo grafico.
  - Salvataggio configurazioni via `POST /settings.json`.

### WiFi

- **Modalità normale (STA)**:
  - Si connette alla rete salvata in `settings.json` o default.
  - Aggiorna orario tramite NTP se internet disponibile.
- **Modalità AP (fallback)**:
  - SSID: `ESP32-Umiditech`, password: `12345678`.
  - IP di default: `192.168.4.1`.
  - Web UI accessibile a client collegati all’AP.

### Allarmi e controlli hardware

- Umidità >65% → LED acceso, buzzer attivo.
- Volume buzzer regolabile tramite potenziometro.
- Errori DHT11 e SD loggati su seriale e `/ESP32.log`.

---

## File richiesti sulla SD
```yaml
index.html
settings.html
script.js
chart.js
style.css
favicon.ico
immagini/ (icona, gif)
fonts/
settings.json (opzionale, per configurazione WiFi)
```

---

## Istruzioni

1. Formatta la SD in FAT32.
2. Copia i file nella root della SD.
3. Inserisci SD nell’ESP32 e alimenta la board.
4. Apri Serial Monitor (115200 baud) per log di avvio, stato WiFi e SD.
5. Collegati all’IP mostrato sul display (rete normale) o a `192.168.4.1` se AP.

**PlatformIO comandi utili:**

```bash
pio run                 # Compila
pio run --target upload # Carica sulla board
pio run --target clean  # Pulisce la build
```