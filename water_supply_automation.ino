#include "DHT.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <PubSubClient.h>

#define DHTPIN 15      // Pin where the DHT11 is connected
#define DHTTYPE DHT11  // DHT 11
#define SOIL_PIN 34    // Analog pin for Soil Moisture Sensor
#define RELAY 5        // Relay pin

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "";
const char* password = "";
const String endpoint = "https://smart-planting.vercel.app/api/record";

// MQTT
// const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client12345")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Relay Pin First
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);  // Turn OFF relay at startup (assuming active LOW relay)

  dht.begin();
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilMoisture = analogRead(SOIL_PIN);

  Serial.print("Raw Soil Moisture Value: ");
  Serial.println(soilMoisture);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C | Humidity: ");
    Serial.print(humidity);
    Serial.print(" % | Soil Moisture: ");
    Serial.println(soilMoisture);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(endpoint);
      http.addHeader("Content-Type", "application/json");

      String payload = "{";
      payload += "\"position\":\"Malabe\",";
      payload += "\"temperature\":" + String(temperature, 1) + ",";
      payload += "\"humidity\":" + String(humidity, 1) + ",";
      payload += "\"moisture\":" + String(soilMoisture);
      payload += "}";

      int httpResponseCode = http.POST(payload);

      if (httpResponseCode == 201) {
        String response = http.getString();
        Serial.println("Response: " + response);
      } else {
        Serial.print("Error sending POST: ");
        Serial.println(httpResponseCode);
      }

      http.end();

      // 💧 Motor Control Based on Moisture
      if (soilMoisture > 2500) {
        Serial.println("Soil is dry. Pumping water for 5 seconds...");
        digitalWrite(RELAY, LOW);   // Turn ON motor (Active LOW)
        delay(5000);                // Run motor for 5 seconds
        digitalWrite(RELAY, HIGH);  // Turn OFF motor
        Serial.println("Pump OFF");
      } else {
        Serial.println("Soil is wet. No need to water.");
      }

      // MQTT
      if (!client.connected()) {
        reconnect();
      }
      client.loop();

      client.publish("smartplanting/temperature", String(temperature, 1).c_str());
      client.publish("smartplanting/humidity", String(humidity, 1).c_str());
      client.publish("smartplanting/moisture", String(soilMoisture).c_str());

    } else {
      Serial.println("WiFi not connected, cannot send data");
    }

    delay(5000); // Delay before next reading
  }
}