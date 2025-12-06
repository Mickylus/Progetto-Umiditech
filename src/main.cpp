#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

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
