/*
Autori: Michele Valiati e Federico Chizzoli
Progetto Umiditech

Collegamenti:
--------------------------------------------------------------------------
Lettore SD:
| Vcc		|	5V		|
| GND		|	GND		|
| MOSI		|	GPIO_23	|
| MISO		|	GPIO_19	|
| SCK		|	GPIO_18	|
| CS		|	GPIO_5	|

Sensore DHT11
| Vcc		|	5V		|
| DHT pin	|	GPIO_13	|
| -			|	-		|
| GND		|	GND		|

Display LCD 16x2 I2C
| GND		|	GND		|
| Vcc		|	5V		|
| SDA		|	GPIO_21	|
| SCL		|	GPIO_22	|

Componenti
| Pot		|	GPIO_34	|
| Buzzer	|	GPIO_14	|
| Led		|	GPIO_15	|
--------------------------------------------------------------------------

Struttura SD
|-------------------|
|	SD				|
|-------------------|
| immagini			|
| |- esp32icon.png	|
| |- pixels.gif		|
| fonts				|
| | aristotelica	|
| | panton			|
| ESP32.log			|
| favicon.ico		|
| index.html		|
| script.js			|
| settings.html		|
| settings.json		|
| storico.json		|
| style.css			|
|-------------------|


*/
// Librerie
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
#define POT_PIN 34	// 3.3V
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
// Ultima impostazione manuale dell'ora
unsigned long lastTime=0;
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
// Contatore per l'errore del sensore
int DHTError = 0;
/* Variabili di Stato*/
// Stabilisce se il WiFi è normale o hotspot (non ha accesso al'ora)
bool isOpenWifi = true;
// Stabilisce se il buzzer ha suonato o no
bool BuzzActivated = false;
// Stabilisce se il lettore SD funziona correttamente
bool SDfailed = false;
// Stabilisce se ha già stampato il messaggio id warning
bool warningMessage = false;
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
// Vecchio Volume
int oldVol=0;
// Vecchia scrittura t
float lastTWrite=0;
// Vecchia scrittura 
float lastHWrite=0;
// Calibrazione automatica volume max
float autoMaxVol=0;
// Calibrazione automatica volume min
float autoMinVol=0;
/*Funzioni*/

int connectWiFi();
void aggiornaDati();
String contentType(const String &filename);
void salvaStorico();
void caricaConfig();
void generaStorico();
void aggiornaLog(String);
void setupServer();
void calibraVol();
void checkSDCorrupted();
void setCompileTime();

void setup(){
	// Inizializzo i componenti & porta seriale
	Serial.begin(115200);
	Wire.begin(LCD_SDA,LCD_SCL);
	setCompileTime();
	lcd.init();
	lcd.backlight();
	dht.begin();
	// Avvio progetto
	Serial.println("Avvio progetto Umiditech");
	lcd.setCursor(0,0);
	lcd.print("Avvio in corso..");
	lcd.setCursor(0,1);
	lcd.print("Caricamento...  ");
	delay(1000);
	// Stabilisco se INPUT/OUTPUT
	pinMode(POT_PIN,INPUT);
	pinMode(BUZZ_PIN,OUTPUT);
	pinMode(LED_PIN,OUTPUT);
	analogWrite(BUZZ_PIN,255);
	digitalWrite(LED_PIN,HIGH);
	delay(300);
	analogWrite(BUZZ_PIN,0);
	digitalWrite(LED_PIN,LOW);
	delay(200);
	analogWrite(BUZZ_PIN,255);
	digitalWrite(LED_PIN,HIGH);
	delay(300);
	analogWrite(BUZZ_PIN,0);
	digitalWrite(LED_PIN,LOW);
	SPI.begin(SCK_SD, MISO_SD, MOSI_SD, CS_SD);
	if(!SD.begin(CS_SD,SPI)){
		calibraVol();
		Serial.println("Lettura SD fallita!");
		lcd.setCursor(0,0);
		lcd.print("Lettura SD:        ");
		lcd.setCursor(0,1);
		lcd.print("Fallita :(      ");
		setCompileTime();
		isOpenWifi=false;
		SDfailed=true;
	}else{
		calibraVol();
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
		}
	}
	// Scrittura LCD
	if(millis() - start_point>= 10000){
		if(millis()-lastLcd >= 1000){
			lastLcd=millis();
			float h = dht.readHumidity();
			float t = dht.readTemperature();
			yield();
			float val=analogRead(POT_PIN);
			vol=map(val,autoMinVol,autoMaxVol,0,100);
			if(vol<0){
				vol=0;
			}else if(vol>100){
				vol=100;
			}
			if(isnan(h)){
				h=oldHum;
				t=oldTemp;
				DHTError++;
			}else{
				DHTError=0;
				warningMessage=false;
				oldHum=h;
				oldTemp=t;
			}
			if(h>65){
				lcd.setCursor(0,0);
				lcd.print("Soglia superata!   ");
				lcd.setCursor(0,1);
				lcd.print("U: ");
				lcd.print((int)h);
				lcd.print("% T: ");
				lcd.print(t);
				lcd.print("C");
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
				lcd.print("C");
			}
			if(millis()-buzz_start >= 3500){
				analogWrite(BUZZ_PIN,0);
			}else{
				analogWrite(BUZZ_PIN,map(vol,0,100,0,255));
			}
		}
	}
	if(DHTError>15){
		if(!warningMessage){
			Serial.println("ATTENZIONE Sensore non funzionante!");
			if(!SDfailed){
				aggiornaLog("WARNING Sensore non funzionante!");
			}
			warningMessage=true;
		}
	}

}
// Imposta l'ora manualmente
void setCompileTime(){
	setTime(15, 44, 50, 13, 12, 2025);
}
// Controlla se la SD è corrotta | Non usata
void checkSDCorrupted(){
	if(!SD.exists("/settings.json")){
		unsigned long start_error=millis();
		lcd.setCursor(0,0);
		lcd.print("SD Corrotta...     ");
		lcd.setCursor(0,1);
		lcd.print("Formattare SD    ");
		Serial.println("SD Corrotta!");
		isOpenWifi=false;
		SDfailed=true;
		digitalWrite(LED_PIN,HIGH);
		while(millis()-start_error>2000){
			analogWrite(BUZZ_PIN,255);
			delay(50);
			analogWrite(BUZZ_PIN,0);
			delay(50);
		}
		digitalWrite(LED_PIN,LOW);
	}
}
// Calibro il volume
void calibraVol(){
	float PMW,totMis=0;
	int i;
	lcd.setCursor(0,0);
	lcd.print("Calibrazione...     ");
	lcd.setCursor(0,1);
	lcd.print("                    ");
	delay(1500);
	lcd.setCursor(0,0);
	lcd.print("Imposta il min..   ");
	lcd.setCursor(0,1);
	lcd.print("                   ");
	lcd.setCursor(0,1);
	delay(1500);
	for(i=0;i<16;i++){
		PMW = analogRead(POT_PIN);
		totMis+=PMW;
		delay(50);
		lcd.print(".");
	}
	autoMinVol=totMis/16;
	lcd.setCursor(0,0);
	lcd.print("Valore Impostato");
	lcd.setCursor(0,1);
	lcd.print("Min: ");
	lcd.print((int)autoMinVol);
	lcd.print("              ");
	delay(2000);
	totMis=0;
	lcd.setCursor(0,0);
	lcd.print("Imposta il max..   ");
	lcd.setCursor(0,1);
	delay(1500);
	for(i=0;i<16;i++){
		PMW = analogRead(POT_PIN);
		totMis+=PMW;
		delay(50);
		lcd.print(".");
	}
	autoMaxVol=totMis/16;
	lcd.setCursor(0,0);
	lcd.print("Valore Impostato");
	lcd.setCursor(0,1);
	lcd.print("Max: ");
	lcd.print((int)autoMaxVol);
	lcd.print("            ");
	if(autoMinVol==autoMaxVol){
		autoMinVol=0;
		autoMaxVol=4095;
	}
	delay(2000);
}
// inizializza il server
void setupServer(){
	if(SD.exists("/ESP32.log")){
		SD.remove("/ESP32.log");
	}
	lcd.setCursor(0,0);
	lcd.print("Lettura SD:        ");
	lcd.setCursor(0,1);
	lcd.print("Riuscita!       ");
	Serial.println("SD Aperta.");
	if(connectWiFi()==1){
		lcd.setCursor(0,0);
		lcd.print("SD Corrotta...     ");
		lcd.setCursor(0,1);
		lcd.print("Formattare SD    ");
		Serial.println("SD Corrotta!");
		isOpenWifi=false;
		SDfailed=true;
		setCompileTime();
		return;
	}
	timeClient.begin();
 	timeClient.update();
  	// Imposto l'ora di TimeLib
	if(isOpenWifi){
  		setTime(timeClient.getEpochTime());
	}else{
		setCompileTime();
	}
	// Creo il file log
	File LOG = SD.open("/ESP32.log",FILE_WRITE);
	LOG.close();
	aggiornaLog("Avviato ESP32");
	aggiornaLog("Inizializzata SD");
	// GET per leggere settings.json
	server.on("/settings.json", HTTP_GET, [](AsyncWebServerRequest *request){
		if(SD.exists("/settings.json")){
			request->send(SD, "/settings.json", "application/json");
		} else {
			request->send(404, "application/json", "{}");
		}
	});
	server.on("/dati", HTTP_GET, [](AsyncWebServerRequest *request){
		DynamicJsonDocument doc(256);
		doc["umidita"] = hum;
		doc["temperatura"] = temp;
		doc["volume"] = vol;

		String json;
		serializeJson(doc, json);

		request->send(200, "application/json", json);
	});
	// POST per scrivere settings.json
	server.on("/settings.json", HTTP_POST, [](AsyncWebServerRequest *request){},NULL,[](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
		if(SD.exists("/settings.json")){
			SD.remove("/settings.json");
		}
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
        File f = SD.open("/storico.json", FILE_READ);
		DynamicJsonDocument doc(2048);
		DeserializationError err = deserializeJson(doc, f);
		f.close();
		yield();
		g_count = doc["count"].as<int>();
		if(g_count>MAX_MEM){
			SD.remove("/storico.json");
			g_count=1;
			aggiornaLog("Inizializzato Storico");
		}else{
			salvaStorico();
			aggiornaLog("Letto Storico");
			return;
		}
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();
	yield();

    if(!isnan(h)){
		hum=h;
		temp=t;
	}
	String time ="";
	if(hour()<10){
		time=time+"0"+String(hour())+":";
	}else{
		time=time+String(hour())+":";
	}
	if(minute()<10){
		time=time+"0"+String(minute());
	}else{
		time=time+String(minute());
	}
    DynamicJsonDocument doc(256);
    // Creo array "misurazioni"
    JsonArray misurazioni = doc.createNestedArray("misurazioni");
    // Aggiungo la prima misurazione
    JsonObject entry = misurazioni.createNestedObject();
    entry["umidita"]     = hum;
    entry["temperatura"] = temp;
    entry["time"]        = time;
	yield();

	doc["count"] = g_count;
    // Scrivo JSON sulla SD
    File f = SD.open("/storico.json", FILE_WRITE);
    if(f){
        serializeJson(doc, f);
        f.close();
		yield();
    }
}
// Scrive file sulla SD
void writeFile(const char *path, const String &data) {
	// Cancello il file
	if(SD.exists(path)){
		SD.remove(path);
		yield();
	}
	File f = SD.open(path, FILE_WRITE);
	f.print(data);
	f.close();
	yield();
}
// Salva i dati su storico.json per i grafici
void salvaStorico(){
	if(!SD.exists("/storico.json")){
		return;
	}
    File f = SD.open("/storico.json", FILE_READ);
    if (!f) return;
	yield();

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
	yield();
    if (err) return;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
	yield();

    if(!isnan(h)){
		hum=h;
		oldHum=hum;
		temp=t;
		oldTemp=temp;
	}else{
		hum=oldHum;
		temp=oldTemp;
	}
	String time ="";
	if(hour()<10){
		time=time+"0"+String(hour())+":";
	}else{
		time=time+String(hour())+":";
	}
	if(minute()<10){
		time=time+"0"+String(minute());
	}else{
		time=time+String(minute());
	}
    // Aggiungo nuova misurazione
    JsonArray misurazioni = doc["misurazioni"];
    JsonObject entry = misurazioni.createNestedObject();
    entry["umidita"] = hum;
    entry["temperatura"] = temp;
    entry["time"] = time;
	yield();

	g_count++;
	doc["count"] = g_count;

    // Riscrivo tutto il JSON
    f = SD.open("/storico.json", FILE_WRITE);
    if(f){
        serializeJson(doc, f);
        f.close();
		yield();
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
	yield();
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
	yield();
    if(err){
        Serial.print("Errore parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }
    // Lettura dei valori
    REFRESH_RATE = doc["refresh_rate"].as<int>();
    GRAPH_RATE   = doc["graph_rate"].as<int>();
	yield();
    // Conversioni come prima
    REFRESH_RATE *= 1000;              // da secondi a millisecondi
    GRAPH_RATE *= 60000;             // da minuti a millisecondi
    MAX_MEM = 6 * (60 / (GRAPH_RATE / 60000)); // MAX_MEM calcolato in base al nuovo GRAPH_RATE
    aggiornaLog("Caricate Impostazioni");
}
// Aiuta a caricare i file del server
String contentType(const String &filename){
    if(filename.endsWith(".htm") || filename.endsWith(".html")){
        return "text/html; charset=utf-8";
    }
    if(filename.endsWith(".css")){
        return "text/css; charset=utf-8";
    }
    if(filename.endsWith(".js")){
        return "application/javascript; charset=utf-8";
    }
    if(filename.endsWith(".json")){
        return "application/json; charset=utf-8";
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
    return "text/plain; charset=utf-8";
}
// Aggiorna il log
void aggiornaLog(String message){
	String log_time="["+String(day())+"/"+String(month())+"/"+String(year())+"-";
	if(hour()<10){
		log_time=log_time+"0"+String(hour())+":";
	}else{
		log_time=log_time+String(hour())+":";
	}
	if(minute()<10){
		log_time=log_time+"0"+String(minute())+":";
	}else{
		log_time=log_time+String(minute())+":";
	}
	if(second()<10){
		log_time=log_time+"0"+String(second())+"] ";
	}else{
		log_time=log_time+String(second())+"] "; 
	}
	yield();
	File Log = SD.open("/ESP32.log",FILE_APPEND);
	String log_message = log_time+message+"\n";
	Log.print(log_message);
	Log.close();
	yield();
}
// Aggiorna i valori salvati su dati.json
void aggiornaDati(){
    int val = analogRead(POT_PIN);
    Serial.println(val);
    vol = map(val, autoMinVol, autoMaxVol, 0, 100);
    float h = dht.readHumidity();
    float t = dht.readTemperature();
	yield();
    Serial.print("DHT 11 | Umidità: ");
    Serial.print(h);
    Serial.print("% - Temperatura: ");
    Serial.print(t);
    Serial.println("°");
    // Se i valori sono validi li uso, altrimenti metto 0
    if(isnan(h)){
		hum=oldHum;
	}else{
		hum=h;
		oldHum=hum;
	}
    if(isnan(t)){
		temp=oldTemp;
	}else{
		temp=t;
		oldTemp=t;
	}
    // Limito tra 0 e 100
    vol = constrain(vol, 0, 100);
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
	yield();
	if(err){
		return false;
	}
	ssid = doc["ssid"].as<String>();
	password = doc["password"].as<String>();
	d_ssid = doc["d_ssid"].as<String>();
	d_password = doc["d_password"].as<String>();
	yield();
	return true;
}
// Si connette alle varie reti wifi
int connectWiFi(){
	String ssid, password, d_ssid,d_password,log_message;
	if(!readWifiConfig(ssid,password,d_ssid,d_password)){
		Serial.println("Nessun WiFi configurato.");
		return 1;
	}
	lcd.setCursor(0,0);
	lcd.print("Wifi: ");
	lcd.print(ssid);
	lcd.print("     ");
	Serial.println("Connessione a: " + ssid);
	lcd.setCursor(0,1);
	lcd.print("                   ");
	delay(1000);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid.c_str(), password.c_str());
	lcd.setCursor(0,1);
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
		delay(500);
		yield();
		Serial.print(".");
		lcd.print(".");
	}
	if(WiFi.status() == WL_CONNECTED) {
		Serial.println("\nConnesso!");
		Serial.print("IP: ");
		Serial.println(WiFi.localIP());
		lcd.setCursor(0,0);
		lcd.print("Connesso! IP:      ");
		lcd.setCursor(0,1);
		lcd.print(WiFi.localIP());
		lcd.print("    ");
		log_message="Connesso a "+ssid;
	}else{
		// Riprovo con il wifi di default
		Serial.println("\nConnessione a " + ssid + " fallita!");
		lcd.setCursor(0,0);
		lcd.print("Connessione fallita!");
		lcd.setCursor(0,1);
		lcd.print("                       ");
		delay(500);
		lcd.setCursor(0,0);
		lcd.print("Wifi: ");
		lcd.print(d_ssid);
		lcd.print("    ");
		lcd.setCursor(0,1);
		lcd.print("                 ");
		lcd.setCursor(0,1);
		if(d_ssid=="PC-Mighes"){
			isOpenWifi=false;
		}
		Serial.println("Provo con il WiFi di default\n");
		delay(1000);
		WiFi.mode(WIFI_STA);
		WiFi.begin(d_ssid.c_str(), d_password.c_str());
		start = millis();
		while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
			delay(500);
			yield();
			Serial.print(".");
			lcd.print(".");
		}
		if(WiFi.status() == WL_CONNECTED) {
			Serial.println("\nConnesso!");
			Serial.print("IP: ");
			Serial.println(WiFi.localIP());
			lcd.setCursor(0,0);
			lcd.print("Connesso! IP:      ");
			lcd.setCursor(0,1);
			lcd.print(WiFi.localIP());
			lcd.print("     ");
			log_message="Connesso a "+d_ssid;
		}else{
			Serial.println("Connessione a " + d_ssid +" fallita!");
			lcd.setCursor(0,0);
			lcd.print("Inizio AP        ");
			delay(400);
			WiFi.mode(WIFI_AP_STA);
			WiFi.softAP("ESP32-Umiditech","12345678");
			lcd.setCursor(0,0);
			lcd.print("Connesso AP, IP:          ");
			lcd.setCursor(0,1);
			lcd.print(WiFi.softAPIP());
			Serial.println("Wifi AP iniziato:     ");
			Serial.print("IP: ");
			Serial.println(WiFi.softAPIP());
			isOpenWifi=false;
			log_message="Access point inizializzato!";
		}
	}
	aggiornaLog(log_message);
	return 0;
}
