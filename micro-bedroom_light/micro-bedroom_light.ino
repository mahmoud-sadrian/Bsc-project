#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// WiFi Configuration
const char* ssid = "M.sdr";
const char* password = "12345678";

// Server Configuration
const char* serverHost = "smartify24.ir";
const String serverURL = "https://smartify24.ir/api.php";
const int deviceId = 3;

// API Key for authentication (generated on server)
const char* apiKey = "t9taq56ccbp0fb31a4cg0wcpwg3zzqw7";

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

// SSL Client
WiFiClientSecure *client = nullptr;

void setup() {
  Serial.begin(115200);
  
  pinMode(relayPin, OUTPUT);
  pinMode(statusLed, OUTPUT);
  digitalWrite(relayPin, HIGH); // Start OFF
  digitalWrite(statusLed, LOW);

  // Connect to WiFi
  WiFi.begin(ssid, password);
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
  String url = serverURL + "?action=devices&sub_action=status&device_id=" + String(deviceId);
  
  Serial.println("Checking status from: " + url);

  http.begin(*client, url);
  http.addHeader("Authorization", "Bearer " + String(apiKey));

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      const char* status = doc["status"];
      Serial.print("Device status: ");
      Serial.println(status);

      // Control relay (LOW = ON if active-low)
      if (strcmp(status, "ON") == 0) {
        digitalWrite(relayPin, LOW);
      } else {
        digitalWrite(relayPin, HIGH);
        if (timerActive) {
          timerActive = false;
          timerDuration = 0;
          Serial.println("Timer cancelled");
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
    timerActive = false;
    timerDuration = 0;
    Serial.println("Timer expired - Device turned OFF");
    updateDeviceStatus("OFF", "Timer expired");
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
  String url = serverURL + "?action=devices&sub_action=control&device_id=" + String(deviceId);
  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(apiKey));

  DynamicJsonDocument doc(256);
  doc["status"] = status;
  String jsonString;
  serializeJson(doc, jsonString);

  int httpCode = http.POST(jsonString);
  if (httpCode == HTTP_CODE_OK) {
    logActivity(action);
  } else {
    Serial.print("Status update failed. HTTP Code: ");
    Serial.println(httpCode);
  }
  http.end();
}

// --- Logging ---
void logActivity(String action) {
  String timestamp = timeClient.getFormattedTime();
  Serial.println("Activity: [" + timestamp + "] " + action);
}

// --- Start Timer ---
void startTimer(int minutes) {
  if (minutes <= 0) return;
  timerDuration = minutes * 60;
  timerStartTime = millis();
  timerActive = true;
  digitalWrite(relayPin, LOW);
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