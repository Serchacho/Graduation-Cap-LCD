#include <WiFi.h>

// Network credentials - change these
const char* ssid = "CapESP";
const char* password = "graduation";  // min 8 chars, or "" for open network

// Static IP config for the AP (ESP32 acts as its own router)
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure the AP network before starting it
  WiFi.softAPConfig(local_ip, gateway, subnet);

  // Start the access point
  WiFi.softAP(ssid, password);

  Serial.println("Access Point started");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Nothing yet - just keeping the AP alive
}