
# Progetto Umiditech — Relazione tecnica

## 1. Sommario

Questo progetto implementa una semplice Web App per monitorare tre valori sintetici (umidità, temperatura, volume) usando un ESP32 che monta una scheda SD. Il dispositivo serve una cartella `web` via HTTP e aggiorna periodicamente il file JSON `dati.json` contenente i valori visualizzati dalla pagina web.

## 2. Obiettivi

- Fornire una interfaccia web minima per visualizzare dati in tempo (grafici e valori).
- Usare la SD per servire file statici HTML/CSS/JS e per salvare i dati generati dal microcontrollore.
- Mantenere il codice semplice e facilmente estendibile.

## 3. Componenti hardware

- Scheda: ESP32 (qualsiasi modulo compatibile).
- Memoria: scheda SD collegata via SPI.
- Connessioni SPI usate nel codice (definite in `src/main.cpp`):
	- MOSI: GPIO 15
	- MISO: GPIO 2
	- SCK:  GPIO 14
	- CS:   GPIO 5

Nota: adattare i pin allo specifico modulo/hardware se necessario.

## 4. Panoramica software

- Linguaggio: C++ (Arduino/PlatformIO).
- Librerie principali: `SPI`, `SD`, `WebServer` (incluse nell'ecosistema ESP32 Arduino core).
- File principale: `src/main.cpp` — monta la SD, avvia il server HTTP sulla porta 80, serve i file dalla cartella `/web` sulla SD e scrive periodicamente `/web/dati.json` con i valori.
- La funzione di scrittura su SD è stata semplificata in `writeFile(const char*, const String&)` per sovrascrivere il file con i nuovi dati.

## 5. Interfaccia web

I file dell'interfaccia si trovano in `src/Web/` e includono:
- `index.html` — pagina principale che mostra i valori e i grafici.
- `settings.html` — pagina per impostazioni (SSID/password, frequenza di aggiornamento, checkbox per impostare il wifi come default).
- `style.css` — stili della pagina (contiene regole per i box dei valori e layout responsive).
- `carica_dati.js`, `grafico.js`, `chart.js` — script per caricare `dati.json` e disegnare i grafici.

La pagina è progettata per essere servita direttamente dalla SD: basta copiare la cartella `Web` nella root della SD (o il suo contenuto sotto `/web`) perché il server la trovi.

## 6. Come compilare e caricare

1. Aprire il progetto con PlatformIO o l'IDE Arduino (impostare la board ESP32 corretta).
2. Compilare e caricare su ESP32.
3. Preparare la SD: creare la cartella `/web` e copiare i file `index.html`, `settings.html`, `style.css` e gli script richiesti.
4. Inserire la SD nell'ESP32 e avviare la scheda.

## 7. Uso e comportamento

- All'avvio il firmware monta la SD e avvia il server HTTP.
- Il server risponde alle richieste servendo i file presenti sotto `/web` sulla SD.
- Ogni `WRITE_INTERVAL_MS` (configurato in `main.cpp`) il firmware aggiorna `/web/dati.json` con i valori correnti.

Nota: la versione corrente del firmware non esegue automaticamente la connessione WiFi; se necessario è possibile ripristinare o aggiungere la logica per leggere le credenziali da `/web/wifi.json` e inizializzare la connessione (nel codice originario era prevista tale funzione).

## 8. Struttura del repository

```
platformio.ini
include/
lib/
src/
	main.cpp
	Web/
		index.html
		settings.html
		style.css
		carica_dati.js
		grafico.js
		chart.js
		immagini/
		fonts/
test/
```

## 9. Possibili estensioni future

- Riattivare la lettura automatica delle credenziali WiFi da SD e la funzionalità POST `/wifi.json` per aggiornare le credenziali da browser.
- Migliorare la scrittura atomica su SD per aumentare robustezza (utile se l'alimentazione è instabile).
- Aggiungere autenticazione semplice per l'interfaccia di impostazioni.
- Persistenza dei parametri (filtro, soglie, intervalli) su SD o in NVS.

## 10. Risoluzione problemi comuni

- Se il server non risponde: verificare che la SD sia montata correttamente e che la cartella `/web` esista.
- Se i file non vengono trovati: controllare che i nomi dei file siano corretti e che la SD sia formattata FAT32.
- Errori di scrittura su SD: controllare i collegamenti SPI e il pin CS.

---

Se vuoi, posso: applicare la logica per la connessione WiFi da SD, ripristinare la scrittura atomica su SD, o integrare il salvataggio delle impostazioni via `settings.html`.

