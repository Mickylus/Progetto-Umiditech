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
const unsigned long WRITE_INTERVAL_MS = 5000; // write dati.json every 5s

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