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
#define BUZZ_PIN 27
#define DHT_PIN 13
#define DHT_TYPE DHT11

AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);

DHT dht(DHT_PIN,DHT_TYPE);

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600*1); // offset in secondi (qui +1 ora)

int start_point = 0;
unsigned long lastWrite = 0;
unsigned long lastLcd = 0;
unsigned long lastStoricoWrite = 0;
int REFRESH_RATE = 5000; // write dati.json every 5s
int GRAPH_RATE = 60000;
int g_count = 0;
int MAX_MEM = 288;
bool isOpenWifi = true;

float hum=0;
float temp=0;
int vol=0;

float oldHum=0;
float oldTemp=0;

void connectWiFi();
bool serveFromSD(AsyncWebServerRequest *);
void aggiornaDati();
String contentType(const String &filename);
void salvaStorico();
void caricaConfig();
void generaStorico();

void setup(){
	Serial.begin(115200);
	Wire.begin(LCD_SDA,LCD_SCL);

	lcd.init();
	lcd.backlight();
	lcd.setCursor(0,0);
	lcd.print("Caricamento wifi...");
	Serial.println("Starting ESP32 Async SD server");

	SPI.begin(SCK_SD, MISO_SD, MOSI_SD, CS_SD);
	if(!SD.begin(CS_SD,SPI)){
		Serial.println("SD init failed. Check wiring and CS pin.");
		lcd.setCursor(0,0);
		lcd.print("Lettura SD fallita!");
	}else{
		lcd.setCursor(0,0);
		lcd.print("Lettura SD riuscita!");
		Serial.println("SD initialized.");
		connectWiFi();
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

		timeClient.begin();
  		timeClient.update();

  		// Imposto l'ora di TimeLib
		if(isOpenWifi){
  			setTime(timeClient.getEpochTime());
		}else{
			setTime(16,58,0,8,12,2025); // ore:minuti:secondi, giorno:mese:anno)
		}
		pinMode(POT_PIN,INPUT);
		pinMode(BUZZ_PIN,OUTPUT);
		dht.begin();

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
	start_point=millis();
}

void loop(){
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
	if(millis() - start_point>= 10000){
		if(millis()-lastLcd >= 1000){
		lastLcd=millis();
		lcd.setCursor(0,0);
		lcd.print("Umiditech - Wifi");
		lcd.setCursor(0,1);
		lcd.print("H:");
		lcd.print((int)dht.readHumidity());
		lcd.print("% , T:");
		lcd.print(dht.readTemperature());
		lcd.print("°    ");
		}
	}

}
void generaStorico(){
	if(SD.exists("/storico.json")){
		SD.remove("/storico.json");
	}
	float h = dht.readHumidity();
	float t = dht.readTemperature();
	if(!isnan(h)){
		hum=h;
	}
	if(!isnan(t)){
		temp=t;
	}
	File f = SD.open("/storico.json",FILE_WRITE);
	String timeStamp = "\""+String(hour())+":"+String(minute())+":"+String(second())+"\"";
	String newStoric = "{\"misurazioni\":[{\"umidita\":"+String(hum)+",\"temperatura\":"+String(temp)+",\"time\":"+timeStamp+"}]}";
	f.print(newStoric);
	f.close();
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

void salvaStorico(){
	if(!SD.exists("/storico.json")){
		return;
	}
	File f = SD.open("/storico.json",FILE_READ);
	String s = f.readString();
	f.close();

	s.remove(s.length()-2);

	if(hum=0){
		hum=oldHum;
	}
	if(temp=0){
		temp=oldTemp;
	}
	f = SD.open("/storico.json",FILE_WRITE);
	f.print(s);
	String timeStamp = "\""+String(hour())+":"+String(minute())+":"+String(second())+"\"";
	String add = ",{\"umidita\":"+String(hum)+",\"temperatura\":"+String(temp)+",\"time\":"+timeStamp+"}]}";
	f.print(add);
	f.close();
}

void caricaConfig(){
	if(!SD.exists("/settings.json")){
		return;
	}
	File f = SD.open("/settings.json",FILE_READ);
	String raw = f.readString();
	f.close();

	raw.replace("\n","");
	raw.replace("\r","");
	raw.replace(" ","");

	int refreshStart = raw.indexOf("\"refresh_rate\":");
	int graphStart = raw.indexOf("\"graph_rate\":");

	refreshStart+=15;
	graphStart+=13;

	int refreshEnd = raw.indexOf(",",refreshStart);
	int graphEnd = raw.indexOf("}",graphStart);

	REFRESH_RATE = raw.substring(refreshStart,refreshEnd).toInt();
	REFRESH_RATE *= 1000;
	GRAPH_RATE = raw.substring(graphStart,graphEnd).toInt();
	MAX_MEM = 6*(60/GRAPH_RATE);
	GRAPH_RATE *= 60000;
}
// Aiuta a caricare i file
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

void aggiornaDati(){
	int val=analogRead(POT_PIN);
	Serial.println(val);
	vol=map(val,50,4000,0,100);
	float h=dht.readHumidity();
	float t=dht.readTemperature();
	Serial.println(h);
	Serial.println(t);
	if(!isnan(h)){
		hum=h;
		oldHum=hum;
	}else{
		hum=0;
	}
	if(!isnan(t)){
		temp=t;
		oldTemp=temp;
	}else{
		temp=0;
	}
	if(vol>100){
		vol=100;
	}
	if(vol<0){
		vol=0;
	}
	String json = "{";
	json += "\"umidita\":" + String(hum) + ",";
	json += "\"temperatura\":" + String(temp) + ",";
	json += "\"volume\":" + String(vol);
	json += "}";
	writeFile("/dati.json", json);
}

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

	String raw = f.readString();
	f.close();

	raw.replace("\n", "");
	raw.replace("\r", "");
	raw.replace(" ", "");

	int ssidStart = raw.indexOf("\"ssid\":\"");
	int passStart = raw.indexOf("\"password\":\"");
	int dssidStart = raw.indexOf("\"d_ssid\":\"");
	int dpassStart = raw.indexOf("\"d_password\":\"");

	if (ssidStart < 0 || passStart < 0 || dssidStart < 0 || dpassStart < 0){
		return false;
	}
	ssidStart += 8;  // salto "ssid":"  
	passStart += 12; // salto "password":" 
	dssidStart += 10;
	dpassStart += 14;

	int ssidEnd = raw.indexOf("\"", ssidStart);
	int passEnd = raw.indexOf("\"", passStart);
	int dssisEnd = raw.indexOf("\"", dssidStart);
	int dpassEnd = raw.indexOf("\"", dpassStart);

	ssid = raw.substring(ssidStart, ssidEnd);
	password = raw.substring(passStart, passEnd);
	d_ssid = raw.substring(dssidStart,dssisEnd);
	d_password = raw.substring(dpassStart,dpassEnd);
	return true;
}

void connectWiFi(){
	String ssid, password, d_ssid,d_password;
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
		lcd.print("Connesso! IP:");
		lcd.setCursor(0,1);
		lcd.print(WiFi.localIP());
	}else{
		// Riprovo con il wifi di default
		Serial.println("\nConnessione a " + ssid + " fallita!");
		lcd.setCursor(0,0);
		lcd.print("Connessione fallita!");
		lcd.setCursor(0,0);
		lcd.print("Wifi: ");
		lcd.print(d_ssid);
		lcd.print("    ");
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
			lcd.print("Connesso! IP:");
			lcd.setCursor(0,1);
			lcd.print(WiFi.localIP());
			isOpenWifi=false;
		}else{
			Serial.println("Connessione a " + d_ssid +" fallita!");
		}
	}
}