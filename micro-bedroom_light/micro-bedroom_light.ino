#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server Configuration
const char* serverURL = "http://your-domain.com/api.php";
const int deviceId = 1; // Change this to match your device ID in database

// Hardware Configuration
const int relayPin = D1; // GPIO5 for relay control
const int statusLed = D4; // GPIO2 for status LED (built-in LED)

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000); // UTC+3 for Iran

// Variables
unsigned long lastStatusCheck = 0;
unsigned long statusCheckInterval = 5000; // Check every 5 seconds
unsigned long lastTimerCheck = 0;
unsigned long timerCheckInterval = 1000; // Check timer every second
int timerDuration = 0; // Timer duration in seconds
unsigned long timerStartTime = 0;
bool timerActive = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(relayPin, OUTPUT);
  pinMode(statusLed, OUTPUT);
  digitalWrite(relayPin, LOW); // Start with relay OFF
  digitalWrite(statusLed, LOW);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    digitalWrite(statusLed, !digitalRead(statusLed)); // Blink LED while connecting
  }
  
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Start NTP client
  timeClient.begin();
  timeClient.update();
  
  digitalWrite(statusLed, HIGH); // Solid LED when connected
  
  // Initial status sync
  syncDeviceStatus();
}

void loop() {
  // Update NTP time
  timeClient.update();
  
  unsigned long currentMillis = millis();
  
  // Check device status periodically
  if (currentMillis - lastStatusCheck >= statusCheckInterval) {
    syncDeviceStatus();
    lastStatusCheck = currentMillis;
  }
  
  // Handle timer if active
  if (timerActive) {
    handleTimer();
  }
  
  // Check for timer expiration every second
  if (currentMillis - lastTimerCheck >= timerCheckInterval) {
    checkTimer();
    lastTimerCheck = currentMillis;
  }
  
  delay(100);
}

void syncDeviceStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    String url = String(serverURL) + "?action=devices&sub_action=status&device_id=" + String(deviceId);
    
    Serial.print("Checking status from: ");
    Serial.println(url);
    
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      
      // Parse JSON response
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      String status = doc["status"];
      String timestamp = doc["timestamp"];
      
      Serial.print("Device status: ");
      Serial.println(status);
      Serial.print("Last update: ");
      Serial.println(timestamp);
      
      // Update relay based on status
      if (status == "ON") {
        digitalWrite(relayPin, HIGH);
        Serial.println("Relay turned ON");
      } else {
        digitalWrite(relayPin, LOW);
        Serial.println("Relay turned OFF");
        
        // Cancel any active timer when turned off manually
        if (timerActive) {
          timerActive = false;
          timerDuration = 0;
          Serial.println("Timer cancelled (device turned off)");
        }
      }
      
      // Blink LED to indicate successful communication
      digitalWrite(statusLed, LOW);
      delay(100);
      digitalWrite(statusLed, HIGH);
      
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpCode);
      blinkError(3); // Blink 3 times for HTTP error
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected");
    blinkError(5); // Blink 5 times for WiFi error
  }
}

void handleTimer() {
  if (!timerActive) return;
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = (currentTime - timerStartTime) / 1000; // Convert to seconds
  
  if (elapsedTime >= timerDuration) {
    // Timer expired - turn off device
    digitalWrite(relayPin, LOW);
    timerActive = false;
    timerDuration = 0;
    
    Serial.println("Timer expired - Device turned OFF");
    
    // Update server status
    updateDeviceStatus("OFF", "Timer expired");
  }
}

void checkTimer() {
  // This function runs every second when timer is active
  if (timerActive) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = (currentTime - timerStartTime) / 1000;
    unsigned long remainingTime = timerDuration - elapsedTime;
    
    if (remainingTime <= 0) {
      handleTimer(); // Force timer handling
    }
  }
}

void updateDeviceStatus(String status, String action) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    String url = String(serverURL) + "?action=devices&sub_action=control&device_id=" + String(deviceId);
    
    Serial.print("Updating status to: ");
    Serial.println(status);
    
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(256);
    doc["status"] = status;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpCode = http.POST(jsonString);
    
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Status update response: " + payload);
      
      // Log the action
      logActivity(action);
      
    } else {
      Serial.print("Status update failed. HTTP Code: ");
      Serial.println(httpCode);
    }
    
    http.end();
  }
}

void logActivity(String action) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    // Note: Activity logging is handled automatically by the API
    // This function is for additional logging if needed
    Serial.println("Activity: " + action);
    
    // You can add custom logging here if needed
    String timestamp = timeClient.getFormattedTime();
    Serial.println("[" + timestamp + "] " + action);
  }
}

void startTimer(int minutes) {
  timerDuration = minutes * 60; // Convert to seconds
  timerStartTime = millis();
  timerActive = true;
  
  Serial.print("Timer started for ");
  Serial.print(minutes);
  Serial.println(" minutes");
  
  // Turn on device when timer starts
  digitalWrite(relayPin, HIGH);
  updateDeviceStatus("ON", "Timer started for " + String(minutes) + " minutes");
}

void blinkError(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(statusLed, LOW);
    delay(200);
    digitalWrite(statusLed, HIGH);
    delay(200);
  }
}

// Manual control functions (for testing)
void manualControl(String command) {
  if (command == "ON") {
    digitalWrite(relayPin, HIGH);
    updateDeviceStatus("ON", "Manual control - ON");
    Serial.println("Manual ON");
  } else if (command == "OFF") {
    digitalWrite(relayPin, LOW);
    updateDeviceStatus("OFF", "Manual control - OFF");
    
    // Cancel timer if active
    if (timerActive) {
      timerActive = false;
      timerDuration = 0;
      Serial.println("Timer cancelled (manual off)");
    }
    Serial.println("Manual OFF");
  } else if (command.startsWith("TIMER:")) {
    int minutes = command.substring(6).toInt();
    if (minutes > 0) {
      startTimer(minutes);
    }
  }
}

// Web server for local control (optional)
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<html><body>";
    html += "<h1>ESP8266 Smart Device Control</h1>";
    html += "<p>Device ID: " + String(deviceId) + "</p>";
    html += "<p>Status: " + String(digitalRead(relayPin) ? "ON" : "OFF") + "</p>";
    html += "<p><a href='/on'>Turn ON</a></p>";
    html += "<p><a href='/off'>Turn OFF</a></p>";
    html += "<form action='/timer' method='POST'>";
    html += "<input type='number' name='minutes' min='1' max='1440' placeholder='Minutes'>";
    html += "<input type='submit' value='Set Timer'>";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  server.on("/on", HTTP_GET, []() {
    manualControl("ON");
    server.send(200, "text/plain", "Device turned ON");
  });
  
  server.on("/off", HTTP_GET, []() {
    manualControl("OFF");
    server.send(200, "text/plain", "Device turned OFF");
  });
  
  server.on("/timer", HTTP_POST, []() {
    if (server.hasArg("minutes")) {
      int minutes = server.arg("minutes").toInt();
      if (minutes > 0) {
        startTimer(minutes);
        server.send(200, "text/plain", "Timer set for " + String(minutes) + " minutes");
      }
    }
  });
  
  server.begin();
  Serial.println("Web server started");
}

// Uncomment the following line in setup() to enable web server:
// setupWebServer();

// And add this to loop() if using web server:
// server.handleClient();