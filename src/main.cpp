#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// WiFi belépési azonosítók
const char* ssid = "DESKTOP1";
const char* password = "veressgabor";

// Prometheus szerver adatok
const char* prometheus_server = "192.168.137.1";  // Prometheus URL
const int prometheus_port = 9090;                        // Prometheus Port
const char* query = "/api/v1/query?query=esp8266_total_current"; // Most az összegzett áramot kérdezi le, majd módosítani, hogy más adatokat is lekérdezzen

// ESP8266 HTTP Server port 8663
ESP8266WebServer server(8663);

// Variables for storing metrics
float current0 = 1.2;          // "current0" példa érték
float current1 = 2.5;          // "current1" példa érték
float connection = 1.0;       // "connection" példa érték
float prometheusValue = 0.0;  // Lekérdezett érték tárolásához

// /metrics endpoint kezelése
void sendMetricsToEndpoint() {

  // Promtheus formátum létrehozása
  String metrics = "";
  metrics += "# HELP esp8266_current Current sensor reading.\n";
  metrics += "# TYPE esp8266_current gauge\n";
  metrics += "esp8266_current0 " + String(current0, 2) + "\n";

  metrics += "# HELP esp8266_current Current sensor reading.\n";
  metrics += "# TYPE esp8266_current gauge\n";
  metrics += "esp8266_current1 " + String(current1, 2) + "\n";
  
  metrics += "# HELP esp8266_connection Connection metric value.\n";
  metrics += "# TYPE esp8266_connection gauge\n";
  metrics += "esp8266_connection " + String(connection, 2) + "\n";
  
  // Metrikák küldése prometheusnak
  server.send(200, "text/plain", metrics);
}

// Prometheus lekérdezése és "prometheusValue" frissítése
void queryPrometheus() {
  // WiFi csatalkozás ellenörzése
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping query.");
    return;
  }

  // Prometheus URL
  String url = String("http://") + prometheus_server + ":" + prometheus_port + query;

  // Szükséges WiFi és http clientek létrehozása
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url); //HTTP API
  
  int httpCode = http.GET();  // Kérés

  // Kód ellenörzése
  if (httpCode > 0) {
    // Header kezelése
    Serial.printf("GET request to %s: [HTTP Code: %d]\n", url.c_str(), httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Response from Prometheus:");
      Serial.println(payload);  // Adat JSON-be nyomtatása

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print(F("JSON deserialization failed: "));
        Serial.println(error.c_str());
        return;
      }

      // Adat kinyerése a JSON fileból
      const char* status = doc["status"];
      if (String(status) == "success") {
        // Stringbe kinyerés
        const char* valueStr = doc["data"]["result"][0]["value"][1];
        // stringből float-á alakítás
        prometheusValue = String(valueStr).toFloat();

        // kiírás ellenörzés céljából
        Serial.print("Extracted Prometheus Value: ");
        Serial.println(prometheusValue);
      }
    }
  } else {
    Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();  // Lecsatalkozás
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // WiFi csatalkozás
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start the HTTP server
  server.on("/metrics", sendMetricsToEndpoint);  // /metrics létrehozása
  server.begin();
  Serial.println("HTTP server started on port 8266");

  // lekérdezés
  queryPrometheus();
}

void loop() {
  // HTTP kliens kérések kezelése
  server.handleClient();

  // 30másodpercenként lekérdezés ismétlése
  static unsigned long lastQueryTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastQueryTime >= 30000) {
    queryPrometheus();
    lastQueryTime = currentTime;
  }
}
