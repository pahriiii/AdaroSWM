#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_task_wdt.h> // Tambahkan library untuk watchdog timer

#define RXD2 16
#define TXD2 17
#define LED LED_BUILTIN

const char* ssid = "Deviota";
const char* password = "iotelkafi";
// const char* mqtt_server = "51.15.243.241";
// const int mqtt_port = 1883;
// const char* mqtt_username = "iotelkafi";
// const char* mqtt_password = "jvnwQu3wt5njx2rM";
// const char* mqtt_client_id = "swm";

const char* mqtt_server = "us.loclx.io";
const int mqtt_port = 4411;
const char* mqtt_username = "pahri";
const char* mqtt_password = "emangbuatapa";
const char* mqtt_client_id = "swm";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(921600);
  Serial2.begin(921600, SERIAL_8N1, RXD2, TXD2);
  pinMode(LED, OUTPUT);
  pinMode(LED, HIGH);

  esp_task_wdt_init(120, true);
  
  // Koneksi ke jaringan Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");
  
  // Koneksi ke broker MQTT
  client.setServer(mqtt_server, mqtt_port);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
  blinkLed(3);
}

void loop() {
  // Refresh watchdog timer
  esp_task_wdt_reset();
  
  if (!client.connected()) {
    reconnect();
  }
  
  // Membaca data yang dikirimkan melalui port serial
  if (Serial2.available() > 0) {
    String receivedString = Serial2.readStringUntil('\n');

    // Kirim data yang dibaca ke topik MQTT
    client.publish("swm001", receivedString.c_str());

    blinkLed(2);
  }
  
  client.loop();
}

void reconnect() {
  static long lastReconnectAttempt = 0;
  static const long reconnectInterval = 5000; // Atur interval pengulangan sesuai kebutuhan

  if (millis() - lastReconnectAttempt > reconnectInterval) {
    lastReconnectAttempt = millis();
    Serial.println("Reconnecting to MQTT...");
    if (client.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT, rc=");
      Serial.println(client.state());
    }
  }
}

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
  }
}
