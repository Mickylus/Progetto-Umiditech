#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TimeLib.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>


// Pin SD
#define MOSI_SD 23
#define MISO_SD 19
#define SCK_SD  18
#define CS_SD   5
// Pin Display
#define LCD_SCL 22
#define LCD_SDA 21
// Pin Sensori
#define POT_PIN 34
#define BUZZ_PIN 14
#define DHT_PIN 13
#define DHT_TYPE DHT11
#define LED_PIN 15

AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);

DHT dht(DHT_PIN,DHT_TYPE);

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600*1); // offset in secondi (qui +1 ora)

/*Variabili di tempo*/
// Ultima scrittura dati
unsigned long lastWrite = 0;
// Ultima stampa su LCD
unsigned long lastLcd = 0;
// Ultima scrittura grafico
unsigned long lastStoricoWrite = 0;
// Tempo di inizio del suono del buzzer
unsigned long buzz_start = 0;
/*Variabili contatori*/
// Segna l'inizio ufficiale del programma (fuori dal setup)
int start_point = 0;
// Periodo di scrittura dati
int REFRESH_RATE = 5000;
// Periodo di scrittura grafico
int GRAPH_RATE = 60000;
// Conteggio scrittura grafico
int g_count = 0;
// Massima grandezza grafico
int MAX_MEM = 288;
/* Variabili di Stato*/
// Stabilisce se il WiFi è normale o hotspot (non ha accesso al'ora)
bool isOpenWifi = true;
// Stabilisce se il buzzer ha suonato o no
bool BuzzActivated = false;
// Stabilisce se il lettore SD funziona correttamente
bool SDfailed = false;
/*Variabili sensori*/
// Umidità (DHT 11)
float hum=0;
// Temperatura (DHT 11)
float temp=0;
// Volume (Potenziometro)
int vol=0;
// Vecchia Umidità
float oldHum=0;
// Vecchia temperatura
float oldTemp=0;
/*Funzioni*/

void connectWiFi();
void aggiornaDati();
String contentType(const String &filename);
void salvaStorico();
void caricaConfig();
void generaStorico();
void aggiornaLog(String);
void setupServer();

void setup(){
	// Inizializzo i componenti & porta seriale
	Serial.begin(115200);
	Wire.begin(LCD_SDA,LCD_SCL);
	lcd.init();
	lcd.backlight();
	dht.begin();
	// Avvio progetto
	Serial.println("Avvio progetto Umiditech");
	lcd.setCursor(0,0);
	lcd.print("Avvio in corso..");
	lcd.setCursor(0,1);
	lcd.print("Caricamento...  ");
	// Stabilisco se INPUT/OUTPUT
	pinMode(POT_PIN,INPUT);
	pinMode(BUZZ_PIN,OUTPUT);
	pinMode(LED_PIN,OUTPUT);
	SPI.begin(SCK_SD, MISO_SD, MOSI_SD, CS_SD);
	if(!SD.begin(CS_SD,SPI)){
		Serial.println("Lettura SD fallita!");
		lcd.setCursor(0,0);
		lcd.print("ERRORE:         ");
		lcd.setCursor(0,1);
		lcd.print("SD error        ");
		//			    ORE			 MINUTI			  SECONDI
		setTime(atoi(__TIME__),atoi(__TIME__+3),atoi(__TIME__+6),13,12,2025);
		isOpenWifi=false;
		SDfailed=true;
	}else{
		setupServer();
		start_point=millis();
	}
}

void loop(){
	// Scrittura dati server
	if(!SDfailed){
		// Aggiorno i dati ogni 2 secondi se sono cambiati per evitare l'usura della microSD
		if (millis() - lastWrite >= REFRESH_RATE) {
			lastWrite = millis();
			aggiornaDati();
		}
		if(millis()-lastStoricoWrite >= GRAPH_RATE){
			if(isOpenWifi){
				if(timeClient.update()){
					setTime(timeClient.getEpochTime());
				}
			}
			lastStoricoWrite=millis();
			if(g_count>=MAX_MEM){
				generaStorico();
				g_count=0;
			}else{
				salvaStorico();
			}
			g_count++;
		}
	}
	// Scrittura LCD
	if(millis() - start_point>= 10000){
		if(millis()-lastLcd >= 1000){
			lastLcd=millis();
			float h = dht.readHumidity();
			float t = dht.readTemperature();
			float val=analogRead(POT_PIN);
			vol=map(val,100,4000,0,100);
			if(vol<0){
				vol=0;
			}else if(vol>100){
				vol=100;
			}
			if(isnan(h)){
				h=0;
				t=0;
			}
			if(h>65){
				lcd.setCursor(0,0);
				lcd.print("Soglia superata!   ");
				lcd.setCursor(0,1);
				lcd.print("U: ");
				lcd.print((int)h);
				lcd.print("% T: ");
				lcd.print(t);
				lcd.print("°");
				if(!BuzzActivated){
					BuzzActivated=true;
					buzz_start=millis();
				}
				digitalWrite(LED_PIN,HIGH);
			}else{
				BuzzActivated=false;
				digitalWrite(LED_PIN,LOW);
				lcd.setCursor(0,0);
				lcd.print("Umiditech  ");
				String time=String(hour())+":";
				if(minute()<10){
					time=time+"0"+String(minute())+" ";
				}else{
					time=time+String(minute())+"";
				}
				lcd.print(time);
				lcd.setCursor(0,1);
				lcd.print("U: ");
				lcd.print((int)h);
				lcd.print("% T: ");
				lcd.print(t);
				lcd.print("°");
			}
			if(millis()-buzz_start >= 3500){
				analogWrite(BUZZ_PIN,0);
			}else{
				analogWrite(BUZZ_PIN,map(vol,0,100,0,255));
			}
		}
	}
}
// inizializza il server
void setupServer(){
	if(SD.exists("/ESP32.log")){
		SD.remove("/ESP32.log");
	}
	lcd.setCursor(0,0);
	lcd.print("Lettura SD riuscita!");
	Serial.println("SD Aperta.");
	connectWiFi();
	timeClient.begin();
  	timeClient.update();
  	// Imposto l'ora di TimeLib
	if(isOpenWifi){
  		setTime(timeClient.getEpochTime());
	}else{
		setTime(atoi(__TIME__),atoi(__TIME__+3),atoi(__TIME__+6),13,12,2025); // ore:minuti:secondi, giorno:mese:anno)
	}
	// Creo il file log
	File LOG = SD.open("/ESP32.log",FILE_WRITE);
	String log_time="["+String(day())+"/"+String(month())+"/"+String(year())+"-"+String(hour())+":"+String(minute())+":"+String(second())+"] ";
	String log_message=log_time+"Avviato ESP32 - Umiditech\n"+log_time+"Inizializzata SD\n";
	LOG.print(log_message);
	LOG.close();
	// GET per leggere settings.json
	server.on("/settings.json", HTTP_GET, [](AsyncWebServerRequest *request){
		if(SD.exists("/settings.json")){
			request->send(SD, "/settings.json", "application/json");
		} else {
			request->send(404, "application/json", "{}");
		}
	});
	// POST per scrivere settings.json
	server.on("/settings.json", HTTP_POST, [](AsyncWebServerRequest *request){},NULL,[](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
		File f = SD.open("/settings.json", FILE_WRITE);
		if(f){
			f.write(data, len);
			f.close();
			caricaConfig();
			lcd.setCursor(0,0);
			lcd.print("Nuove Impostazioni:");
			lcd.setCursor(0,1);
			lcd.print("Rr: ");
			lcd.print(REFRESH_RATE/1000);
			lcd.print(" Gr: ");
			lcd.print(GRAPH_RATE/60000);
			lcd.print("     ");
			request->send(200, "application/json", "{\"status\":\"ok\"}");
		} else {
			request->send(500, "application/json", "{\"status\":\"error\"}");
		}
	});
	// Fornisce la pagina web
	server.onNotFound([](AsyncWebServerRequest *request){
		String path = request->url();
		if(path.endsWith("/")) path += "index.html"; 
		if(SD.exists(path)){
			AsyncWebServerResponse *response = request->beginResponse(SD, path, contentType(path));
			response->addHeader("Cache-Control", "max-age=3600"); // cache per 1 ora
			request->send(response);
		} else {
			request->send(404, "text/plain", "File Not Found");
		}
	});
	server.begin();
	Serial.println("HTTP server started");
	caricaConfig();
	generaStorico();
}
// Genera l'inizio del file storico.json
void generaStorico(){
	if(SD.exists("/storico.json")) {
        SD.remove("/storico.json");
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if(!isnan(h)){
		hum=h;
		temp=t;
	}
    DynamicJsonDocument doc(256);
    // Creo array "misurazioni"
    JsonArray misurazioni = doc.createNestedArray("misurazioni");
    // Aggiungo la prima misurazione
    JsonObject entry = misurazioni.createNestedObject();
    entry["umidita"]     = hum;
    entry["temperatura"] = temp;
    entry["time"]        = String(hour()) + ":" + String(minute()) + ":" + String(second());
    // Scrivo JSON sulla SD
    File f = SD.open("/storico.json", FILE_WRITE);
    if(f){
        serializeJson(doc, f);
        f.close();
    }
    aggiornaLog("Inizializzato Storico");
}
// Scrive file sulla SD
void writeFile(const char *path, const String &data) {
	// Cancello il file
	if(SD.exists(path)){
		SD.remove(path);
	}
	File f = SD.open(path, FILE_WRITE);
	f.print(data);
	f.close();
}
// Salva i dati su storico.json per i grafici
void salvaStorico(){
	if(!SD.exists("/storico.json")){
		return;
	}
    File f = SD.open("/storico.json", FILE_READ);
    if (!f) return;

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if(!isnan(h)){
		hum=h;
		oldHum=hum;
		temp=t;
		oldTemp=temp;
	}else{
		hum=oldHum;
		temp=oldTemp;
	}

    // Aggiungo nuova misurazione
    JsonArray misurazioni = doc["misurazioni"];
    JsonObject entry = misurazioni.createNestedObject();
    entry["umidita"] = hum;
    entry["temperatura"] = temp;
    entry["time"] = String(hour()) + ":" + String(minute()) + ":" + String(second());

    // Riscrivo tutto il JSON
    f = SD.open("/storico.json", FILE_WRITE);
    if(f){
        serializeJson(doc, f);
        f.close();
    }

    String qty = "[" + String(g_count) + "/" + String(MAX_MEM) + "]";
    String message = "Salvati dati su storico " + qty;
    aggiornaLog(message);
}
// Carica le impostazioni
void caricaConfig(){
	if(!SD.exists("/settings.json")){
        return;
    }
    File f = SD.open("/settings.json", FILE_READ);
    if(!f){
		return;
	}
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if(err){
        Serial.print("Errore parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }
    // Lettura dei valori
    REFRESH_RATE = doc["refresh_rate"].as<int>();
    GRAPH_RATE   = doc["graph_rate"].as<int>();
    // Conversioni come prima
    REFRESH_RATE *= 1000;              // da secondi a millisecondi
    GRAPH_RATE *= 60000;             // da minuti a millisecondi
    MAX_MEM = 6 * (60 / (GRAPH_RATE / 60000)); // MAX_MEM calcolato in base al nuovo GRAPH_RATE
    aggiornaLog("Caricate Impostazioni");
}
// Aiuta a caricare i file del server
String contentType(const String &filename){
	if(filename.endsWith(".htm") || filename.endsWith(".html")){
		return "text/html";
	}
	if(filename.endsWith(".css")){
		return "text/css";
	}
	if(filename.endsWith(".js")){
		return "application/javascript";
	}
	if(filename.endsWith(".json")){
		return "application/json";
	}
	if(filename.endsWith(".png")){
		return "image/png";
	}
	if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")){
		return "image/jpeg";
	}
	if (filename.endsWith(".ico")){
		return "image/x-icon";
	}
	if(filename.endsWith(".ttf")){
		return "font/ttf";
	}
	if(filename.endsWith(".woff")){
		return "font/woff";
	}
	return "text/plain";
}
// Aggiorna il log
void aggiornaLog(String message){
	String log_time="["+String(day())+"/"+String(month())+"/"+String(year())+"-"+String(hour())+":"+String(minute())+":"+String(second())+"] ";
	File Log = SD.open("/ESP32.log",FILE_APPEND);
	String log_message = log_time+message+"\n";
	Log.print(log_message);
	Log.close();
}
// Aggiorna i valori salvati su dati.json
void aggiornaDati(){
    int val = analogRead(POT_PIN);
    Serial.println(val);
    vol = map(val, 50, 4000, 0, 100);
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.print("DHT 11 | Umidità: ");
    Serial.print(h);
    Serial.print("% - Temperatura: ");
    Serial.print(t);
    Serial.println("°");
    // Se i valori sono validi li uso, altrimenti metto 0
    if(isnan(h)){
		hum=0;
	}else{
		hum=h;
	}
    if(isnan(t)){
		temp=0;
	}else{
		temp=t;
	}
	if(hum!=0){
		oldHum = hum;
    	oldTemp = temp;
	}
    // Limito tra 0 e 100
    vol = constrain(vol, 0, 100);
    // ---- ArduinoJson ----
    DynamicJsonDocument doc(256);
    doc["umidita"] = hum;
    doc["temperatura"] = temp;
    doc["volume"] = vol;
    // Serializzo il JSON
    String json;
    serializeJson(doc, json);
    // Salvo nel file
    writeFile("/dati.json", json);
}
// Legge le impostazioni del wifi
bool readWifiConfig(String &ssid, String &password, String &d_ssid, String &d_password){
	if(!SD.exists("/settings.json")){
		Serial.println("settings.json non trovato");
		return false;
	}
	File f = SD.open("/settings.json", FILE_READ);
	if (!f){
		Serial.println("Impossibile aprire settings.json");
		return false;
	}
	DynamicJsonDocument doc(256);
	DeserializationError err = deserializeJson(doc, f);
	f.close();

	if(err){
		return false;
	}
	ssid = doc["ssid"].as<String>();
	password = doc["password"].as<String>();
	d_ssid = doc["d_ssid"].as<String>();
	d_password = doc["d_password"].as<String>();
	return true;
}
// Si connette alle varie reti wifi
void connectWiFi(){
	String ssid, password, d_ssid,d_password,log_message;
	if(!readWifiConfig(ssid,password,d_ssid,d_password)){
		Serial.println("Nessun WiFi configurato.");
		return;
	}
	lcd.setCursor(0,0);
	lcd.print("Wifi: ");
	lcd.print(ssid);
	Serial.println("Connessione a: " + ssid);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid.c_str(), password.c_str());
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
		delay(500);
		Serial.print(".");
	}
	if(WiFi.status() == WL_CONNECTED) {
		Serial.println("\nConnesso!");
		Serial.print("IP: ");
		Serial.println(WiFi.localIP());
		lcd.setCursor(0,0);
		lcd.print("Connesso! IP:      ");
		lcd.setCursor(0,1);
		lcd.print(WiFi.localIP());
		log_message="Connesso a "+ssid;
	}else{
		// Riprovo con il wifi di default
		Serial.println("\nConnessione a " + ssid + " fallita!");
		lcd.setCursor(0,0);
		lcd.print("Connessione fallita!");
		lcd.setCursor(0,0);
		lcd.print("Wifi: ");
		lcd.print(d_ssid);
		lcd.print("    ");
		if(d_ssid=="PC-Mighes"){
			isOpenWifi=false;
		}
		Serial.println("Provo con il WiFi di default\n");
		WiFi.mode(WIFI_STA);
		WiFi.begin(d_ssid.c_str(), d_password.c_str());
		start = millis();
		while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
			delay(500);
			Serial.print(".");
		}
		if(WiFi.status() == WL_CONNECTED) {
			Serial.println("\nConnesso!");
			Serial.print("IP: ");
			Serial.println(WiFi.localIP());
			lcd.setCursor(0,0);
			lcd.print("Connesso! IP:      ");
			lcd.setCursor(0,1);
			lcd.print(WiFi.localIP());
			log_message="Connesso a "+d_ssid;
		}else{
			Serial.println("Connessione a " + d_ssid +" fallita!");
			lcd.setCursor(0,0);
			lcd.print("Inizio AP        ");
			WiFi.mode(WIFI_AP_STA);
			WiFi.softAP("ESP32-Umiditech","12345678");
			lcd.setCursor(0,0);
			lcd.print("Connesso AP, IP:          ");
			lcd.setCursor(0,1);
			lcd.print(WiFi.localIP());
			Serial.println("Wifi AP iniziato:     ");
			Serial.print("IP: ");
			Serial.println(WiFi.localIP());
			isOpenWifi=false;
			log_message="Access point inizializzato!";
		}
	}
	aggiornaLog(log_message);
}