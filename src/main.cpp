#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// Pin SD
#define MOSI_SD 15
#define MISO_SD 2
#define SCK_SD 14
#define CS_SD 5

AsyncWebServer server(80);

unsigned long lastWrite = 0;
const unsigned long WRITE_INTERVAL_MS = 2000; // write dati.json every 2s

// Synthetic sensor values
int hum = 34;
int temp = 23;
int vol = 60;

int old_hum = 0;
int old_temp = 0;
int old_vol = 0;

void setup(){
    Serial.begin(9600);
    Serial.println("Starting ESP32 Async SD server");

    SPI.begin(SCK_SD, MISO_SD, MOSI_SD);
    if(!SD.begin(CS_SD, SPI)) {
        Serial.println("SD init failed. Check wiring and CS pin.");
    }else{
        Serial.println("SD initialized.");
    }
    connectWiFi();
    // Fornisce la pagina web
    server.onNotFound([](AsyncWebServerRequest *request){
        if(!serveFromSD(request)){
            request->send(404,"text/plain","File Not Found");
        }
    });
    server.begin();
    Serial.println("HTTP server started");
    if(!SD.exists("/dati.json")){
        aggiornaDati();
    }
}

void loop(){
    // Aggiorno i dati ogni 2 secondi se sono cambiati per evitare l'usura della microSD
    if (millis() - lastWrite >= WRITE_INTERVAL_MS) {
        lastWrite = millis();
        if((old_hum!=hum) || (old_temp!=temp) || (old_vol!=vol)){
            old_hum=hum;
            old_temp=temp;
            old_vol=vol;
            aggiornaDati();
        }
    }
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
    return "text/plain";
}

// Serve file dalla SD
bool serveFromSD(AsyncWebServerRequest *request){
    File file = SD.open("/web/index.html", FILE_READ);
    if (!file) return false;

    request->send(SD, "/web/index.html", contentType("/web/index.html"));
    return true;
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

void aggiornaDati() {
    hum = (hum + 1) % 101;
    temp = (temp + 1) % 101;
    String json = "{";
    json += "\"umidita\":" + String(hum) + ",";
    json += "\"temperatura\":" + String(temp) + ",";
    json += "\"volume\":" + String(vol);
    json += "}";
    writeFile("/dati.json", json);
}

bool readWifiConfig(String &ssid, String &password, String &d_ssid, String &d_password){
    if(!SD.exists("/web/settings.json")){
        Serial.println("settings.json non trovato");
        return false;
    }
    File f = SD.open("/web/settings.json", FILE_READ);
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
    String ssid, password, dssid,dpassword;
    if(!readWifiConfig(ssid,password,dssid,dpassword)){
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
        Serial.println("\nConnessione a " + ssid + " fallita!");
        Serial.println("Provo con il WiFi di default\n");
        WiFi.mode(WIFI_STA);
        WiFi.begin(dssid.c_str(), dpassword.c_str());
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
            Serial.println("Connessione a " + dssid +" fallita!");
        }
    }
}