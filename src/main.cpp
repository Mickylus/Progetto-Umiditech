#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Wire.h>


// Pin SD
#define MOSI_SD 23
#define MISO_SD 19
#define SCK_SD  18
#define CS_SD   5

AsyncWebServer server(80);

unsigned long lastWrite = 0;
unsigned long lastStoricoWrite = 0;
int REFRESH_RATE = 5000; // write dati.json every 5s
int GRAPH_RATE = 50000;

// Synthetic sensor values
int hum = 34;
int temp = 23;
int vol = 60;

int old_hum = 0;
int old_temp = 0;
int old_vol = 0;

void connectWiFi();
bool serveFromSD(AsyncWebServerRequest *);
void aggiornaDati();
String contentType(const String &filename);
void salvaStorico();
void caricaConfig();

void setup(){
	Serial.begin(115200);
	Serial.println("Starting ESP32 Async SD server");

	SPI.begin(SCK_SD, MISO_SD, MOSI_SD, CS_SD);
	if(!SD.begin(CS_SD,SPI)){
		Serial.println("SD init failed. Check wiring and CS pin.");
	}else{
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
		if(!SD.exists("/dati.json")){
			aggiornaDati();
		}
		caricaConfig();
	}
}

void loop(){
	// Aggiorno i dati ogni 2 secondi se sono cambiati per evitare l'usura della microSD
	if (millis() - lastWrite >= REFRESH_RATE) {
		lastWrite = millis();
		if((old_hum!=hum) || (old_temp!=temp) || (old_vol!=vol)){
			old_hum=hum;
			old_temp=temp;
			old_vol=vol;
			aggiornaDati();
		}
	}
	if(millis()-lastStoricoWrite >= GRAPH_RATE){
		lastStoricoWrite=millis();
		salvaStorico();
	}
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
    String raw = "";
    if(!SD.exists("/storico.json")){
        return;
    }

    File f = SD.open("/storico.json", FILE_READ);
    while(f.available()){
        raw += (char)f.read();
    }
    f.close();

    raw.replace("\n",""); raw.replace("\r",""); raw.replace(" ","");

    String arrayStr;
    int start = raw.indexOf("[");
    int end = raw.indexOf("]");
    arrayStr = raw.substring(start + 1, end);

    // Rimuovi primo elemento se ci sono già 59
    int count = 0;
    for(int i=0;i<arrayStr.length();i++){
        if(arrayStr[i] == '}') count++;
    }
    if(count >= 59){
        int firstEnd = arrayStr.indexOf("}") + 1;
        arrayStr = arrayStr.substring(firstEnd + 1);
        if(arrayStr[0] == ',') arrayStr = arrayStr.substring(1); // rimuove eventuale virgola iniziale
    }

    // Aggiungi nuovo elemento
    if(arrayStr.length() > 0) arrayStr += ",";
    arrayStr += "{\"umidita\":" + String(hum) + ",\"temperatura\":" + String(temp) + "}";

    String nuovoJson = "{\"misurazioni\":[" + arrayStr + "]}";

    SD.remove("/storico.json");
    f = SD.open("/storico.json", FILE_WRITE);
    f.print(nuovoJson);
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
	GRAPH_RATE *= 10000;

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
	return "text/plain";
}

void aggiornaDati(){
	hum++;
	temp++;
	if(hum>100){
		hum=0;
	}
	if(temp>100){
		temp=0;
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
	}else{
		// Riprovo con il wifi di default
		Serial.println("\nConnessione a " + ssid + " fallita!");
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
		}else{
			Serial.println("Connessione a " + d_ssid +" fallita!");
		}
	}
}