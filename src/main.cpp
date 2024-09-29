#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// WiFi belépési azonosítók
const char* ssid = "DESKTOP1";
const char* password = "veressgabor";

// Prometheus server details
const char* prometheus_server = "192.168.137.1";  // Replace with your Prometheus server IP or URL
const int prometheus_port = 9090;                        // Default Prometheus port
const char* query = "/api/v1/query?query=esp8266_total_current";

// ESP8266 HTTP Server on port 8663
ESP8266WebServer server(8663);

// Variables for storing metrics
float current0 = 1.2;          // Example value for "current0"
float current1 = 2.5;          // Example value for "current1"
float connection = 1.0;       // Example value for "connection"
float prometheusValue = 0.0;  // Store the queried value from Prometheus

// Function to handle /metrics endpoint for Prometheus scraping
void handleMetrics() {

  // Prepare the Prometheus-compatible metrics format
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
  
  // Send the metrics to Prometheus
  server.send(200, "text/plain", metrics);
}

// Function to query Prometheus and update `prometheusValue` variable
void queryPrometheus() {
  // Check Wi-Fi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping query.");
    return;
  }

  // Create the URL for the Prometheus query
  String url = String("http://") + prometheus_server + ":" + prometheus_port + query;

  // Use WiFiClient and HTTPClient to make the request
  WiFiClient client;  // Create WiFiClient object
  HTTPClient http;
  http.begin(client, url);  // Use the new API: http.begin(WiFiClient, URL)
  
  int httpCode = http.GET();  // Send the request

  // Check the returning code
  if (httpCode > 0) {
    // HTTP header has been sent and server response header has been handled
    Serial.printf("GET request to %s: [HTTP Code: %d]\n", url.c_str(), httpCode);

    // File found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Response from Prometheus:");
      Serial.println(payload);  // Print the returned data from Prometheus

      // Parse the JSON response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print(F("JSON deserialization failed: "));
        Serial.println(error.c_str());
        return;
      }

      // Access the JSON structure to get the value
      const char* status = doc["status"];
      if (String(status) == "success") {
        // Extract the value as a string
        const char* valueStr = doc["data"]["result"][0]["value"][1];
        // Convert the extracted string value to float and update `prometheusValue`
        prometheusValue = String(valueStr).toFloat();

        // Output the result
        Serial.print("Extracted Prometheus Value: ");
        Serial.println(prometheusValue);
      }
    }
  } else {
    Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();  // Close connection
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to Wi-Fi
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
  server.on("/metrics", handleMetrics);  // Define the /metrics route
  server.begin();
  Serial.println("HTTP server started on port 8266");

  // Initial query to Prometheus
  queryPrometheus();
}

void loop() {
  // Handle HTTP client requests
  server.handleClient();

  // Periodically query Prometheus every 30 seconds
  static unsigned long lastQueryTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastQueryTime >= 30000) {  // 30 seconds interval
    queryPrometheus();
    lastQueryTime = currentTime;
  }
}
