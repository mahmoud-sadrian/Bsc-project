#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "secrets.h"

// Server Configuration
const String serverURL = "https://" + String(SERVER_HOST) + "/api.php";

// Hardware Configuration
const int relayPin = D1;
const int statusLed = D4;

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);

// Variables
unsigned long lastStatusCheck = 0;
unsigned long statusCheckInterval = 5000;
unsigned long lastTimerCheck = 0;
unsigned long timerCheckInterval = 1000;
int timerDuration = 0;
unsigned long timerStartTime = 0;
bool timerActive = false;
String currentDeviceStatus = "OFF";
int userId = USER_ID; // User ID from secrets.h

// SSL Client
WiFiClientSecure *client = nullptr;

void setup() {
  Serial.begin(115200);
  
  pinMode(relayPin, OUTPUT);
  pinMode(statusLed, OUTPUT);
  digitalWrite(relayPin, HIGH); // Start OFF (assuming active-low relay)
  digitalWrite(statusLed, LOW);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    digitalWrite(statusLed, !digitalRead(statusLed));
  }
  
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Start NTP
  timeClient.begin();
  timeClient.update();
  
  digitalWrite(statusLed, HIGH);

  // Initialize secure client
  client = new WiFiClientSecure;
  if (client) {
    client->setInsecure(); // For Let's Encrypt SSL
  }

  // Sync immediately
  syncDeviceStatus();
}

void loop() {
  timeClient.update();
  unsigned long currentMillis = millis();

  // Sync device status periodically
  if (currentMillis - lastStatusCheck >= statusCheckInterval) {
    syncDeviceStatus();
    lastStatusCheck = currentMillis;
  }

  // Handle timer
  if (timerActive) {
    handleTimer();
  }

  // Check timer every second
  if (currentMillis - lastTimerCheck >= timerCheckInterval) {
    checkTimer();
    lastTimerCheck = currentMillis;
  }

  delay(100);
}

// --- Sync Device Status ---
void syncDeviceStatus() {
  if (WiFi.status() != WL_CONNECTED || client == nullptr) {
    Serial.println("Cannot sync: WiFi not connected");
    blinkError(5);
    return;
  }

  HTTPClient http;
  String url = serverURL + "?action=devices&sub_action=status&device_id=" + String(DEVICE_ID);
  
  Serial.println("Checking status from: " + url);

  http.begin(*client, url);
  http.addHeader("Authorization", "Bearer " + String((const char*)API_KEY));

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      const char* status = doc["status"];
      const char* deviceId = doc["device_id"];
      
      Serial.print("Device status: ");
      Serial.println(status);
      Serial.print("Device ID: ");
      Serial.println(deviceId);

      // Control relay (LOW = ON if active-low)
      if (strcmp(status, "ON") == 0) {
        digitalWrite(relayPin, LOW);
        currentDeviceStatus = "ON";
      } else {
        digitalWrite(relayPin, HIGH);
        currentDeviceStatus = "OFF";
        
        // Cancel timer if device is turned off externally
        if (timerActive) {
          timerActive = false;
          timerDuration = 0;
          Serial.println("Timer cancelled by external command");
        }
      }

      // Blink LED on success
      digitalWrite(statusLed, LOW);
      delay(100);
      digitalWrite(statusLed, HIGH);
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      blinkError(4);
    }
  } else if (httpCode == 401) {
    Serial.println("Authentication failed - Invalid API key");
    blinkError(6);
  } else if (httpCode == 404) {
    Serial.println("Device not found on server");
    blinkError(7);
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    blinkError(3);
  }

  http.end();
}

// --- Handle Timer ---
void handleTimer() {
  if (!timerActive) return;
  
  unsigned long elapsedTime = (millis() - timerStartTime) / 1000;
  if (elapsedTime >= timerDuration) {
    digitalWrite(relayPin, HIGH);
    currentDeviceStatus = "OFF";
    timerActive = false;
    timerDuration = 0;
    Serial.println("Timer expired - Device turned OFF");
    
    // Update server about timer expiration
    updateDeviceStatus("OFF", "Timer expired after " + String(timerDuration/60) + " minutes");
  }
}

void checkTimer() {
  if (timerActive) {
    unsigned long remainingTime = timerDuration - (millis() - timerStartTime) / 1000;
    if (remainingTime <= 0) {
      handleTimer();
    }
  }
}

// --- Update Device Status ---
void updateDeviceStatus(String status, String action) {
  if (WiFi.status() != WL_CONNECTED || client == nullptr) return;

  HTTPClient http;
  String url = serverURL + "?action=devices&sub_action=control&device_id=" + String(DEVICE_ID);
  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String((const char*)API_KEY));

  DynamicJsonDocument doc(256);
  doc["status"] = status;
  String jsonString;
  serializeJson(doc, jsonString);

  int httpCode = http.POST(jsonString);
  if (httpCode == HTTP_CODE_OK) {
    logActivity(action);
    currentDeviceStatus = status;
  } else {
    Serial.print("Status update failed. HTTP Code: ");
    Serial.println(httpCode);
    Serial.print("Response: ");
    Serial.println(http.getString());
  }
  http.end();
}

// --- Logging ---
void logActivity(String action) {
  String timestamp = timeClient.getFormattedTime();
  Serial.println("Activity: [" + timestamp + "] " + action);
  
  // Log to server
  HTTPClient http;
  String url = serverURL + "?action=devices&sub_action=log&device_id=" + String(DEVICE_ID);
  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String((const char*)API_KEY));

  DynamicJsonDocument doc(256);
  doc["action"] = action;
  String jsonString;
  serializeJson(doc, jsonString);

  int httpCode = http.POST(jsonString);
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Log upload failed. HTTP Code: ");
    Serial.println(httpCode);
  }
  http.end();
}

// --- Start Timer ---
void startTimer(int minutes) {
  if (minutes <= 0) return;
  
  timerDuration = minutes * 60;
  timerStartTime = millis();
  timerActive = true;
  digitalWrite(relayPin, LOW);
  currentDeviceStatus = "ON";
  
  Serial.println("Timer started for " + String(minutes) + " minutes");
  updateDeviceStatus("ON", "Timer set for " + String(minutes) + " minutes");
}

// --- Blink Error ---
void blinkError(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(statusLed, LOW);
    delay(200);
    digitalWrite(statusLed, HIGH);
    delay(200);
  }
}