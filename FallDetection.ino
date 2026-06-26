#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define I2C_SDA 4
#define I2C_SCL 5
#define BUZZER_PIN 3
#define PAIR_BUTTON_PIN 9

const char* BACKEND_URL = "https://eldora-backend-production.up.railway.app";
const char* FIRMWARE_VERSION = "dorashield-0.2.0";

Adafruit_MPU6050 mpu;
WebServer server(80);
Preferences prefs;

String deviceKey;
String pairingToken;
String savedSsid;
String savedPassword;
bool setupMode = false;
bool sensorOk = false;
bool fallArmed = true;
unsigned long lastHeartbeatMs = 0;
unsigned long lastTelemetryMs = 0;
unsigned long lastCommandPollMs = 0;
unsigned long lastImpactMs = 0;
float peakAccelerationG = 0;
String impactSeverity = "low";
String motionLevel = "none";

String randomToken() {
  uint32_t a = esp_random();
  uint32_t b = esp_random();
  char buf[17];
  snprintf(buf, sizeof(buf), "%08lx%08lx", (unsigned long)a, (unsigned long)b);
  return String(buf);
}

String macSuffix() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return mac.substring(mac.length() - 6);
}

String jsonStatus() {
  StaticJsonDocument<512> doc;
  doc["productName"] = "ELDORA_DORASHIELD";
  doc["deviceType"] = "dorashield";
  doc["deviceKey"] = deviceKey;
  doc["pairingToken"] = pairingToken;
  doc["setupSsid"] = "ELDORA-SHIELD-" + macSuffix();
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  doc["wifiSsid"] = WiFi.isConnected() ? WiFi.SSID() : savedSsid;
  doc["wifiRssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  doc["localIp"] = WiFi.isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["batteryLevel"] = nullptr;
  doc["isCharging"] = false;
  doc["sensorStatus"] = sensorOk ? "ok" : "error";
  String body;
  serializeJson(doc, body);
  return body;
}

void sendJson(int status, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(status, "application/json", body);
}

void handleStatus() {
  sendJson(200, jsonStatus());
}

void handleWifiScan() {
  int n = WiFi.scanNetworks();
  StaticJsonDocument<2048> doc;
  JsonArray networks = doc.createNestedArray("networks");
  for (int i = 0; i < n; i++) {
    JsonObject item = networks.createNestedObject();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void saveWifi(const String& ssid, const String& password) {
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  savedSsid = ssid;
  savedPassword = password;
}

bool connectWifi(unsigned long timeoutMs = 15000) {
  if (savedSsid.length() == 0) return false;
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
}

void handleWifiConfig() {
  if (server.method() == HTTP_OPTIONS) {
    sendJson(200, "{}");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    sendJson(400, "{\"message\":\"Invalid JSON\"}");
    return;
  }
  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  if (ssid.length() == 0) {
    sendJson(400, "{\"message\":\"SSID required\"}");
    return;
  }
  saveWifi(ssid, password);
  bool connected = connectWifi();
  StaticJsonDocument<256> response;
  response["connected"] = connected;
  response["localIp"] = connected ? WiFi.localIP().toString() : "";
  String body;
  serializeJson(response, body);
  sendJson(200, body);
}

void startSetupPortal() {
  setupMode = true;
  WiFi.mode(WIFI_AP_STA);
  String ssid = "ELDORA-SHIELD-" + macSuffix();
  WiFi.softAP(ssid.c_str());
  server.on("/", handleStatus);
  server.on("/status", handleStatus);
  server.on("/wifi/scan", handleWifiScan);
  server.on("/wifi", handleWifiConfig);
  server.onNotFound(handleStatus);
  server.begin();
}

void addDeviceHeaders(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);
}

void postJson(const String& path, const String& body) {
  if (!WiFi.isConnected()) return;
  HTTPClient http;
  http.begin(String(BACKEND_URL) + path);
  addDeviceHeaders(http);
  http.POST(body);
  http.end();
}

void sendHeartbeat() {
  StaticJsonDocument<512> doc;
  doc["wifiSsid"] = WiFi.SSID();
  doc["wifiRssi"] = WiFi.RSSI();
  doc["localIp"] = WiFi.localIP().toString();
  doc["localPairingToken"] = pairingToken;
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  String body;
  serializeJson(doc, body);
  postJson("/iot/heartbeat", body);
}

void sendLiveTelemetry() {
  StaticJsonDocument<512> doc;
  doc["wifiRssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
  doc["peakAcceleration"] = peakAccelerationG;
  doc["impactSeverity"] = impactSeverity;
  doc["motionLevel"] = motionLevel;
  doc["sensorStatus"] = sensorOk ? "ok" : "error";
  doc["uptimeSeconds"] = millis() / 1000;
  doc["inactivityAfterImpactMs"] = lastImpactMs > 0 ? millis() - lastImpactMs : 0;
  String body;
  serializeJson(doc, body);
  postJson("/iot/telemetry/live", body);
}

void sendFallEvent(float accelerationG) {
  StaticJsonDocument<256> doc;
  doc["confidence"] = min(1.0f, accelerationG / 6.0f);
  String body;
  serializeJson(doc, body);
  postJson("/iot/events/fall", body);
}

void pollCommands() {
  if (!WiFi.isConnected()) return;
  HTTPClient http;
  http.begin(String(BACKEND_URL) + "/iot/commands");
  http.addHeader("X-Device-Key", deviceKey);
  int code = http.GET();
  if (code == 200) {
    String response = http.getString();
    if (response.indexOf("activate_local_alarm") >= 0) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(1200);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  http.end();
}

void updateMotion(float accelerationMs2) {
  float accelerationG = accelerationMs2 / 9.80665f;
  peakAccelerationG = max(peakAccelerationG * 0.96f, accelerationG);
  if (accelerationG < 1.25f) motionLevel = "low";
  else if (accelerationG < 2.0f) motionLevel = "normal";
  else motionLevel = "high";

  if (accelerationG >= 3.5f) impactSeverity = "critical";
  else if (accelerationG >= 2.8f) impactSeverity = "high";
  else if (accelerationG >= 2.0f) impactSeverity = "medium";
  else impactSeverity = "low";

  if (accelerationG > 2.8f && fallArmed) {
    lastImpactMs = millis();
    fallArmed = false;
    digitalWrite(BUZZER_PIN, HIGH);
    sendFallEvent(accelerationG);
  }

  if (accelerationG < 1.6f) {
    digitalWrite(BUZZER_PIN, LOW);
  }
  if (!fallArmed && millis() - lastImpactMs > 10000) {
    fallArmed = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(PAIR_BUTTON_PIN, INPUT_PULLUP);

  prefs.begin("dorashield", false);
  deviceKey = prefs.getString("deviceKey", "");
  if (deviceKey.length() == 0) {
    deviceKey = "dorashield-" + macSuffix();
    prefs.putString("deviceKey", deviceKey);
  }
  pairingToken = prefs.getString("pairToken", "");
  if (pairingToken.length() == 0) {
    pairingToken = randomToken();
    prefs.putString("pairToken", pairingToken);
  }
  savedSsid = prefs.getString("ssid", "");
  savedPassword = prefs.getString("pass", "");

  Wire.begin(I2C_SDA, I2C_SCL);
  sensorOk = mpu.begin();
  if (sensorOk) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  }

  startSetupPortal();
  connectWifi(8000);
}

void loop() {
  server.handleClient();

  if (digitalRead(PAIR_BUTTON_PIN) == LOW) {
    pairingToken = randomToken();
    prefs.putString("pairToken", pairingToken);
    startSetupPortal();
    delay(800);
  }

  if (sensorOk) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float totalAccel = sqrt(
      a.acceleration.x * a.acceleration.x +
      a.acceleration.y * a.acceleration.y +
      a.acceleration.z * a.acceleration.z
    );
    updateMotion(totalAccel);
  }

  if (!WiFi.isConnected() && savedSsid.length() > 0) {
    connectWifi(1000);
  }

  if (WiFi.isConnected() && millis() - lastHeartbeatMs > 60000) {
    lastHeartbeatMs = millis();
    sendHeartbeat();
  }

  if (WiFi.isConnected() && millis() - lastTelemetryMs > 1000) {
    lastTelemetryMs = millis();
    sendLiveTelemetry();
  }

  if (WiFi.isConnected() && millis() - lastCommandPollMs > 5000) {
    lastCommandPollMs = millis();
    pollCommands();
  }

  delay(100);
}
