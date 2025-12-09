
# Progetto Umiditech — Relazione tecnica

## 1. Sommario

Questa relazione documenta l'architettura hardware e software del progetto "Umiditech" nella versione corrente del firmware (`src/main.cpp`). Oltre al server web che serve le pagine da una scheda SD, il firmware gestisce sensori (DHT11), un display I2C, NTP, storicizzazione su SD e la configurazione WiFi tramite `settings.json` sulla SD.

## 2. Componenti hardware e collegamenti

- Scheda: ESP32 (DevKit o equivalente).
- Memoria: scheda SD (FAT32) collegata via bus SPI.
- Display: LCD I2C (LiquidCrystal_I2C) su SDA/SCL.
- Sensore: DHT11 su pin dedicato.
- Potenziometro: ingressso analogico per regolare il volume del buzzer.
- Buzzer e LED per allarmi/indicazioni.

Pin usati nel progetto (definiti in `src/main.cpp`):

- SD MOSI: `GPIO 23` (`MOSI_SD`)
- SD MISO: `GPIO 19` (`MISO_SD`)
- SD SCK : `GPIO 18` (`SCK_SD`)
- SD CS  : `GPIO 5`  (`CS_SD`)
- LCD SDA: `GPIO 21` (`LCD_SDA`)
- LCD SCL: `GPIO 22` (`LCD_SCL`)
- POT    : `GPIO 34` (`POT_PIN`) analog input
- BUZZ   : `GPIO 27` (`BUZZ_PIN`) PWM output
- DHT    : `GPIO 13` (`DHT_PIN`)
- LED    : `GPIO 15` (`LED_PIN`)

Nota: i pin SPI possono variare a seconda del modulo ESP32; verificare e aggiornare le macro in `main.cpp` se necessario.

Schema di collegamento (sintetico):

- SD MOSI -> ESP32 GPIO23
- SD MISO -> ESP32 GPIO19
- SD SCK  -> ESP32 GPIO18
- SD CS   -> ESP32 GPIO5
- LCD SDA -> GPIO21, LCD SCL -> GPIO22
- DHT DATA -> GPIO13
- POT -> ingresso analogico GPIO34
- Buzzer -> GPIO27, LED -> GPIO15

## 3. Panoramica delle funzioni principali (firmware)

Il firmware si trova in `src/main.cpp`. Componenti e comportamenti principali:

- Inizializzazione periferiche: seriale 115200, I2C per LCD, SPI per SD, sensore DHT, NTP client.
- Montaggio SD: `SD.begin(CS_SD, SPI)` e accesso a file in root della SD.
- Server web asincrono: `AsyncWebServer server(80)` che serve file dalla SD quando la richiesta non corrisponde ad altri endpoint (handler `onNotFound`).
- Endpoint HTTP per configurazione:
  - `GET /settings.json` — restituisce il file `/settings.json` dalla SD (application/json) o `{}`/404 se non presente.
  - `POST /settings.json` — scrive il body su `/settings.json`, ricarica la configurazione (`caricaConfig()`), aggiorna LCD e risponde con JSON di stato.
- Gestione WiFi: funzione `connectWiFi()` legge credenziali da `/settings.json` (ssid/password e fallback d_ssid/d_password). Tenta la connessione; se fallisce prova il fallback; in caso estremo avvia un AP (`ESP32-Umiditech`) con password `12345678`.
- Aggiornamento dati: `aggiornaDati()` legge DHT, potenziometro e costruisce un JSON con i campi `umidita`, `temperatura`, `volume` e lo salva su `/dati.json` tramite `writeFile()`.
- Storicizzazione: `generaStorico()` crea `/storico.json` con la prima misurazione; `salvaStorico()` appende nuovi campioni periodicamente.
- Visualizzazione su LCD: aggiornamenti periodici con orario e valori sensore; su soglia di umidità alta (`>65%`) attiva LED e buzzer.

Funzioni helper notevoli:
- `contentType(const String &filename)` — restituisce l'header `Content-Type` corretto in base all'estensione.
- `writeFile(const char *path, const String &data)` — cancella il file esistente e riscrive il contenuto su SD (non atomica).
- `caricaConfig()` — analizza `/settings.json` per parametri come `refresh_rate` e `graph_rate` e aggiorna le variabili runtime (`REFRESH_RATE`, `GRAPH_RATE`, `MAX_MEM`).

## 4. Dati e formato

- `dati.json` (posizione: root della SD): contiene l'ultimo campione con struttura JSON:

  { "umidita": <float>, "temperatura": <float>, "volume": <int> }

- `settings.json` (root della SD): file JSON usato per configurare parametri e credenziali. Campi previsti (esempi):

  {
	"refresh_rate": 5,     // in secondi
	"graph_rate": 1,       // in minuti
	"ssid": "rete1",
	"password": "pwd1",
	"d_ssid": "fallback",
	"d_password": "fallbackpwd"
  }

  Nota: `caricaConfig()` fa parsing semplice (rimuove spazi/newline) e cerca stringhe chiave; mantenere il formato coerente.

- `storico.json` (root della SD): JSON contenente un array di misurazioni con timestamp, creato/aggiornato dalle funzioni `generaStorico()` e `salvaStorico()`.

## 5. Struttura e ruoli dei file in `src/`

- `main.cpp` — firmware ESP32 (montaggio SD, gestione sensori, LCD, NTP, server HTTP, scrittura `dati.json` e `storico.json`).
- `index.html` — interfaccia principale (grafici/valori) (copiare in root della SD per essere servita direttamente).
- `settings.html` — pagina lato client per modificare impostazioni; può inviare POST a `/settings.json`.
- `style.css`, `script.js`, `chart.js` — risorse client per UI e grafici.
- `dati.json`, `settings.json`, `storico.json` — file JSON usati dal firmware e dalla UI (posizionati nella root della SD).
- `immagini/`, `fonts/` — risorse statiche referenziate dalle pagine web.

## 6. Collegamento tra Web UI e firmware

- La Web UI effettua fetch periodici di `/dati.json` (path root) per aggiornare interfaccia e grafici.
- Il server web sull'ESP serve i file direttamente dalla SD (se la richiesta corrisponde a un file presente). L'handler `onNotFound` costruisce il percorso richiesto e, se esiste, serve il file con il `Content-Type` corretto e l'header `Cache-Control: max-age=3600`.
- Per aggiornare le impostazioni da browser: `settings.html` può inviare un `POST /settings.json`; il firmware salva il file su SD e ricarica la configurazione.

## 7. Istruzioni pratiche per test e deployment

1. Formatta la SD in FAT32 e copia i file web nella root della SD (non è necessario il prefisso `/web` nella versione corrente): `index.html`, `settings.html`, `style.css`, `script.js`, `chart.js`, `favicon.ico`, `immagini/`, `fonts/`.
2. Inserisci la SD nell'ESP32 e alimenta la board.
3. Apri il monitor seriale a 115200 baud per i messaggi di debug (esito montaggio SD, avvio server, scritture su `dati.json`, stato WiFi).
4. Se hai impostato WiFi in `settings.json`, il firmware tenterà la connessione; se fallisce, proverà il fallback e infine avvierà un AP `ESP32-Umiditech`.
5. Apri un browser e punta all'indirizzo IP dell'ESP (se connesso) o collegati all'AP per accedere all'interfaccia.

Comandi rapidi (PlatformIO):

```
pio run                 # Compila
pio run --target upload # Carica sulla board (configurata in platformio.ini)
pio run --target clean  # Pulisce la build
```

## 8. Considerazioni sulla robustezza

- Scrittura su SD: `writeFile()` cancella e riscrive il file; non è atomica. Per prevenire corruzione di file in caso di interruzione di alimentazione, implementare una scrittura atomica con file temporaneo (`.tmp`) e rename.
- Controlli: verificare il valore di ritorno di `SD.begin()` e degli `open()` dei file; il firmware logga su seriale e aggiorna l'LCD per stato.
- Parsing JSON: il parsing in `caricaConfig()` è testuale e sensibile al formato; mantenere `settings.json` semplice e coerente.

## 9. Note operative e possibili miglioramenti

- Migliorare la robustezza delle scritture su SD (atomic rename).
- Usare una libreria JSON completa sul firmware per parsing/scrittura più robusti.
- Aggiungere endpoint di diagnostica e / o autenticazione per la pagina `settings.html`.
- Implementare meccanismi di retry più sofisticati per WiFi e gestione del tempo NTP.

---