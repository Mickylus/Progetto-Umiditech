
# Progetto Umiditech — Relazione tecnica
## 1. Sommario

Questa relazione descrive in dettaglio l'architettura hardware e software del progetto "Umiditech", i collegamenti elettrici, le funzioni principali del firmware e il ruolo dei file presenti nella cartella `src/`. Lo scopo è rendere il progetto facilmente replicabile, manutenibile e estendibile.

## 2. Componenti hardware e collegamenti

- Scheda: ESP32 (modulo DevKit o simile).
- Memoria: scheda SD (FAT32) collegata via bus SPI.

Connessioni SPI implementate nel firmware (vedi `src/main.cpp`):

| Segnale | Pin ESP32 (nel progetto) | Uso |
|--------:|:------------------------:|:--- |
| MOSI    | GPIO 15                 | Dati MOSI verso SD |
| MISO    | GPIO 2                  | Dati MISO dalla SD |
| SCK     | GPIO 14                 | Clock SPI |
| CS      | GPIO 5                  | Chip select SD |

Nota: questi pin sono definiti in `main.cpp` con le macro `MOSI_SD`, `MISO_SD`, `SCK_SD`, `CS_SD`. Se il tuo modulo ESP32 usa pin differenti, aggiorna i valori corrispondenti.

Schema di collegamento (semplificato):

- SD MOSI -> ESP32 GPIO15
- SD MISO -> ESP32 GPIO2
- SD SCK  -> ESP32 GPIO14
- SD CS   -> ESP32 GPIO5
- 3.3V e GND condivisi

Consiglio: usare un adattatore SD card/SD socket con livelli logici corretti (3.3V) e provare il semplice sketch di test SD prima di eseguire il firmware.

## 3. Panoramica delle funzioni principali (firmware)

Il file `src/main.cpp` implementa le funzionalità principali. Di seguito le parti più importanti e il loro comportamento.

- `contentType(const String &filename)`
	- Ritorna l'header `Content-Type` corretto in base all'estensione del file (html, css, js, json, png, jpg, ico).

- `serveFromSD(const String &uri)`
	- Riceve un `uri` (es. `/index.html`) e costruisce il percorso su SD (`/web/index.html` se necessario).
	- Controlla `SD.exists(path)` e apre il file per la lettura.
	- Invia gli header CORS e `Cache-Control`, quindi streama il file tramite `server.streamFile()`.

- `handleNotFound()`
	- Handler impostato con `server.onNotFound(...)`. Se `serveFromSD()` trova il file, la richiesta è servita; altrimenti risponde con 404 e un messaggio semplice.

- `writeFile(const char *path, const String &data)`
	- Semplice helper per sovrascrivere un file su SD: rimuove il file esistente (se presente), apre in scrittura e salva i dati.
	- Ritorna `true` se la scrittura è andata a buon fine.
	- Nota: è una funzione semplice; la versione originale includeva una scrittura atomica via file temporaneo, utile in presenza di interruzioni di alimentazione.

- `aggiornaDati()`
	- Funzione che aggiorna le variabili simulate `umidita`, `temperatura`, `volumeVal` incrementandole e resettandole oltre 100.
	- Costruisce una stringa JSON simile a `{ "umidita": 35, "temperatura": 24, "volume": 60 }` e la scrive su `/web/dati.json` usando `writeFile()`.

- `setup()`
	- Inizializza la seriale, avvia SPI (`SPI.begin(SCK_SD, MISO_SD, MOSI_SD)`), inizializza la SD (`SD.begin(CS_SD, SPI)`), registra gli handler del server (attualmente solo `onNotFound`) e avvia il server HTTP (`server.begin()`).
	- Se `/web/dati.json` non esiste, chiama `aggiornaDati()` per creare il file iniziale.

- `loop()`
	- Gestisce le richieste HTTP (`server.handleClient()`) e ogni `WRITE_INTERVAL_MS` invoca `aggiornaDati()` per aggiornare `dati.json`.

Importante: la versione attuale del firmware non esegue la connessione WiFi automatica: le parti che leggevano `/web/wifi.json` e gestivano POST/OPTIONS per `/wifi.json` sono state rimosse per semplicità. Possono essere reintegrate quando si vuole gestire la configurazione via browser.

## 4. Dati e formato

- `dati.json`
	- Esempio di contenuto generato dal firmware:

```
{ "umidita": 35, "temperatura": 24, "volume": 60 }
```

- `settings.json` e `storico.json`
	- File presenti nella cartella `src/` come esempi o per uso dell'interfaccia web. `settings.json` può contenere preferenze lato client (intervallo di aggiornamento, opzioni) e `storico.json` può memorizzare campionamenti passati se lo si implementa.

## 5. Struttura e ruoli dei file in `src/`

- `main.cpp` — firmware ESP32 (montaggio SD, server HTTP, scrittura `dati.json`).
- `index.html` — interfaccia principale (mostra valori e grafici).
- `settings.html` — form per modificare impostazioni locali (SSID, password, frequenza) e checkbox per "Imposta come default".
- `style.css` — regole CSS per layout, box valori, responsive.
- `script.js` — script lato client che carica `dati.json` e aggiorna la UI; gestisce anche l'invio delle impostazioni se implementato.
- `chart.js` — codice per disegnare i grafici nelle canvas presenti nella pagina.
- `dati.json` — file aggiornato dal firmware con i valori attuali.
- `storico.json` — (opzionale) file per storicizzare le misure.
- `settings.json` — (opzionale) file lato client per memorizzare preferenze.
- `favicon.ico`, `immagini/`, `fonts/` — risorse statiche.

## 6. Collegamento tra Web UI e firmware

- La Web UI periodicamente effettua fetch di `/dati.json` (o di `/web/dati.json` se servita con prefisso) per aggiornare i valori mostrati.
- Il firmware genera e sovrascrive `/web/dati.json` ad intervalli regolari (definiti da `WRITE_INTERVAL_MS` in `main.cpp`).

Se si vuole aggiornare impostazioni via browser, il flusso ideale è:
1. L'utente compila `settings.html` e spunta "Imposta come default".
2. La pagina invia un POST (o salva localmente in `settings.json`).
3. Il firmware riceve la richiesta e salva le credenziali su SD (`/web/wifi.json`) o aggiorna `settings.json`.
4. Il firmware può leggere `/web/wifi.json` al boot e connettersi (funzione `connectWiFiFromSD()` nella versione originaria del codice).

## 7. Istruzioni pratiche per test e deployment

1. Formatta la SD in FAT32 e crea la cartella `/web`.
2. Copia i file web da `src/` nella SD mantenendo la struttura (`index.html`, `settings.html`, `style.css`, `script.js`, `chart.js`, `favicon.ico`, `immagini/`, `fonts/`).
3. Inserisci la SD nell'ESP32 e alimenta la board.
4. Apri il monitor seriale a 115200 baud per vedere i messaggi di debug (esito montaggio SD, avvio server e scritture di `dati.json`).
5. Apri un browser e punta all'indirizzo IP dell'ESP32 (se connesso alla rete) oppure, se non connesso via WiFi, usa un adattatore serial-to-HTTP locale o copia i file dalla SD su un server web per testare l'interfaccia.

Comandi rapidi (PlatformIO):

```
pio run               # Compila
pio run --target upload  # Carica sulla board (board specificata in platformio.ini)
pio run --target clean # Pulisce la build
```

## 8. Considerazioni sulla robustezza

- Scrittura su SD: la funzione `writeFile()` è semplice e funziona nella maggior parte dei casi, ma non è totalmente atomica. Se si teme un'interruzione di alimentazione durante la scrittura, ripristinare la strategia con file temporaneo (`.tmp`) + rename per garantire atomicità.
- Gestione errori: controllare sempre il valore di ritorno di `SD.begin()` e degli `open()` di file; loggare su seriale per debugging.

## 9. Possibili estensioni e miglioramenti

- Reinserire `connectWiFiFromSD()` per leggere `/web/wifi.json` e connettersi automaticamente.
- Implementare endpoint POST `/wifi.json` con CORS per permettere la configurazione via browser.
- Aggiungere autenticazione semplice sulla pagina `settings.html` per impedire modifiche non autorizzate.
- Implementare storicizzazione su `storico.json` e pagine di visualizzazione storica.

---