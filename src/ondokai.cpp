#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <Button2.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Adafruit_GFX.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ESP32Ping.h>
#include <lvgl.h>
#include "lv_conf.h"

#include "media/320x170_esp_attakai_display.h"
#include "media/b320x170_esp_ondokai_display.h"

Preferences preferences;
WiFiManager wifiManager;
TFT_eSPI tft = TFT_eSPI();
String minerIP;
WiFiManagerParameter adressip("adressip", "AdressIP", "192.168.1.53", 15);

Button2 button = Button2(0);
Button2 button2 = Button2(14);

unsigned long lastDataRefresh = 0;
const unsigned long dataRefreshInterval = 10000;
unsigned long lastTimeSync = 0;
const unsigned long timeSyncInterval = 10000;

float hashrate;
float temperature;
float efficiency;
float power;
String minerIPAddress;

float fan0Speed = 0.0;  
float fan0RPM = 0.0;    
float fan1Speed = 0.0;  
float fan1RPM = 0.0; 

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
time_t currentSystemTime = 0;

bool shouldSaveConfig = false;

const int MAX_POINTS = 70;  
float hashrateHistory[MAX_POINTS];  

void saveConfigToPreferences();
void saveConfigCallback();
void setup();
void loop();
void displayScreen();
void updateTime();
void connectToWiFi();
void getMinerData();
void checkPing();
void drawHashrateGraph();

void configModeCallback(WiFiManager* myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigToPreferences() {
  preferences.begin("wifi", false);
  preferences.putString("adressip", adressip.getValue());
  preferences.end();
}

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
  saveConfigToPreferences();
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
  }
}

void updateTime() {
  timeClient.update();
  currentSystemTime = timeClient.getEpochTime();
}

void checkPing() {
  if (Ping.ping(minerIP.c_str())) {
    Serial.println("Ping réussi : Miner accessible");
  } else {
    Serial.println("Ping échoué : Miner non accessible");
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 320, 170, b320x170_esp_ondokai_display);

  wifiManager.addParameter(&adressip);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect("Ondokai", "TicTocNextBlock")) {
    Serial.println("Failed to connect and hit timeout");
    wifiManager.startConfigPortal();
  }

  minerIP = adressip.getValue();
  checkPing();

  button.setClickHandler([](Button2& btn) {
    getMinerData();
    displayScreen();
  });

  button2.setClickHandler([](Button2& btn) {
    wifiManager.resetSettings();
    ESP.restart();
  });
}

void loop() {
  button.loop();
  button2.loop();

  if (millis() - lastDataRefresh >= dataRefreshInterval) {
    getMinerData();
    displayScreen();
    drawHashrateGraph(); 
    lastDataRefresh = millis();
  }

  if (shouldSaveConfig) {
    saveConfigCallback();
    shouldSaveConfig = false;
  }
}

void displayScreen() {

  tft.fillScreen(TFT_WHITE);
  tft.pushImage(0, 0, 320, 170,(uint16_t*) b320x170_esp_attakai_display);
  tft.setCursor(0, 0); 
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);

  float hashrateThs = hashrate / 1e6; 

  tft.setCursor(130, 80);
  tft.printf("%.2f", hashrateThs);  
  tft.setCursor(130, 140);
  tft.printf("%.2f", efficiency);  
  tft.setCursor(250, 140);
  tft.printf("%.2d", (int)power);
  tft.setCursor(230, 80);
  if (temperature < 95) {
    tft.setTextColor(TFT_GREEN);
  } 
  else {
    tft.setTextColor(TFT_RED);
  }
  tft.printf("%.2f", temperature);   

  tft.setTextColor(TFT_BLACK);

  tft.setCursor(250, 140);
  tft.printf("%.2d", (int)power);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(240, 20);
  tft.printf("%s", minerIPAddress.c_str());  

  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(130, 60); 
  tft.printf("Real Hashrate"); 
  tft.setCursor(130, 120); 
  tft.printf("Est. Efficiency"); 
  tft.setCursor(230, 60); 
  tft.printf("Temperature"); 
  tft.setCursor(250, 120); 
  tft.printf("Power"); 

  tft.setCursor(192, 87); 
  tft.printf("Th/s"); 
  tft.setCursor(192, 147); 
  tft.printf("W/Ths"); 
  tft.setCursor(300, 80); 
  tft.printf("C"); 
  tft.setCursor(300, 147); 
  tft.printf("W"); 

tft.setTextSize(1);
tft.setCursor(20, 120); 
tft.printf("Fan Monitor"); 

tft.setTextSize(2);
tft.setCursor(25, 135);  
tft.printf("%.0f%%", fan0Speed);  
tft.setTextSize(1);
tft.setCursor(25, 155);  
tft.printf("%.0f", fan0RPM);  

tft.setTextSize(2);
tft.setCursor(70, 135);  
tft.printf("%.0f%%", fan1Speed);  
tft.setTextSize(1);
tft.setCursor(70, 155);  
tft.printf("%.0f", fan1RPM);  

}
void getMinerData() {
  if (minerIP.length() == 0) {
    Serial.println("Miner IP not set!");
    return;
  }

  WiFiClient client;
  DynamicJsonDocument doc(8192);
  DeserializationError error;
  String payload = "";

  String jsonCommandSummary = "{\"command\":\"summary\"}";
  if (client.connect(minerIP.c_str(), 4028)) {
    client.print(jsonCommandSummary);
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        payload += c;
      }
    }
    client.stop();
    Serial.println("Raw response (summary): " + payload);

    error = deserializeJson(doc, payload);
    if (!error && doc.containsKey("SUMMARY")) {
      JsonObject summary = doc["SUMMARY"][0];
      hashrate = summary["MHS av"].as<float>();
      minerIPAddress = minerIP;

      for (int i = MAX_POINTS - 1; i > 0; i--) {
        hashrateHistory[i] = hashrateHistory[i - 1];
      }
      hashrateHistory[0] = hashrate;
    } else {
      Serial.printf("Failed to parse summary: %s\n", error.c_str());
    }
  } else {
    Serial.println("Connection to miner failed for summary");
  }

  payload = "";
  doc.clear();

  String jsonCommandTemps = "{\"command\":\"temps\"}";
  if (client.connect(minerIP.c_str(), 4028)) {
    client.print(jsonCommandTemps);
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        payload += c;
      }
    }
    client.stop();
    Serial.println("Raw response (temps): " + payload);

    error = deserializeJson(doc, payload);
    if (!error && doc.containsKey("TEMPS")) {
      JsonArray temps = doc["TEMPS"];
      float avgChipTemp = 0.0;
      int count = 0;
      for (JsonObject temp : temps) {
        float chipTemp = temp["Chip"].as<float>();
        avgChipTemp += chipTemp;
        count++;
      }
      if (count > 0) {
        temperature = avgChipTemp / count;  
    } else {
      Serial.printf("Failed to parse temps: %s\n", error.c_str());
    }
  } else {
    Serial.println("Connection to miner failed for temps");
  }

  payload = "";
  doc.clear();

  String jsonCommandTunerStatus = "{\"command\":\"tunerstatus\"}";
  if (client.connect(minerIP.c_str(), 4028)) {
    client.print(jsonCommandTunerStatus);
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        payload += c;
      }
    }
    client.stop();
    Serial.println("Raw response (tunerstatus): " + payload);

    error = deserializeJson(doc, payload);
    if (!error && doc.containsKey("TUNERSTATUS")) {
      JsonObject tunerStatus = doc["TUNERSTATUS"][0];
      float approximateChainPowerConsumption = tunerStatus["ApproximateChainPowerConsumption"].as<float>();
      power = tunerStatus["ApproximateMinerPowerConsumption"].as<float>();

      if (hashrate > 0) {
        efficiency = approximateChainPowerConsumption / (hashrate / 1e6);
      } else {
        efficiency = 0;
      }
    } else {
      Serial.printf("Failed to parse tunerstatus: %s\n", error.c_str());
    }
  } else {
    Serial.println("Connection to miner failed for tunerstatus");
  }

  Serial.printf("Hashrate: %f, Efficiency: %.2f W/Ths, Temperature: %.2f °C, Power: %.2f W, Miner IP: %s\n", hashrate, efficiency, temperature, power, minerIPAddress.c_str());

  payload = "";
  doc.clear();


  String jsonCommandFans = "{\"command\":\"fans\"}";
  if (client.connect(minerIP.c_str(), 4028)) {
    client.print(jsonCommandFans);
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        payload += c;
      }
    }
    client.stop();
    Serial.println("Raw response (fans): " + payload);

    error = deserializeJson(doc, payload);
    if (!error && doc.containsKey("FANS")) {
      fan0RPM = doc["FANS"][0]["RPM"].as<float>();
      fan0Speed = doc["FANS"][0]["Speed"].as<float>();
      fan1RPM = doc["FANS"][1]["RPM"].as<float>();
      fan1Speed = doc["FANS"][1]["Speed"].as<float>();

    } else {
      Serial.printf("Failed to parse fans: %s\n", error.c_str());
    }
  } else {
    Serial.println("Connection to miner failed for fans");
  }
}
}

void drawHashrateGraph() {
  int graphXStart = 30;    // Début du graphique en X
  int graphYStart = 60;   // Début du graphique en Y
  int graphWidth =70;   // Largeur du graphique
  int graphHeight = 50;   // Hauteur du graphique

 
  tft.fillRect(graphXStart, graphYStart, graphWidth, graphHeight, TFT_WHITE);

  
  tft.drawRect(graphXStart, graphYStart, graphWidth, graphHeight, TFT_BLACK);

  
  float maxHashrate = 13e6;  
  float maxTemp = 100.0;     

  
  for (int i = 0; i < MAX_POINTS - 1; i++) {
    int x0 = graphXStart + i;
    int y0 = graphYStart + graphHeight - (hashrateHistory[i] / maxHashrate * graphHeight);
    int x1 = graphXStart + i + 1;
    int y1 = graphYStart + graphHeight - (hashrateHistory[i + 1] / maxHashrate * graphHeight);
    tft.drawLine(x0, y0, x1, y1, TFT_BLUE);  
  }

  
  for (int i = 0; i < MAX_POINTS - 1; i++) {
    int x0 = graphXStart + i;
    int y0 = graphYStart + graphHeight - (temperature / maxTemp * graphHeight);
    int x1 = graphXStart + i + 1;
    int y1 = graphYStart + graphHeight - (temperature / maxTemp * graphHeight);
    tft.drawLine(x0, y0, x1, y1, TFT_YELLOW);  
  }

 
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  
 
  tft.setCursor(graphXStart - 25, graphYStart);
  tft.printf("13TH");
  tft.setCursor(graphXStart - 20, graphYStart + graphHeight - 8);
  tft.printf("0TH");

 
  tft.setCursor(graphXStart + graphWidth + 2, graphYStart);
  tft.printf("100C");
  tft.setCursor(graphXStart + graphWidth + 5, graphYStart + graphHeight - 8);
  tft.printf("0C");
}
