/*
  ESP32 Smart Drying Rack System using GCP
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <ESP32Servo.h>

#define DHTTYPE DHT11

int servoSpeed = 20; 
int lastServoPosition = -1;  // Initialize with an invalid position
bool isFanOn = false; // To check whether the dryer fan is activated
bool isDrying = false; // To check whether in dry mode
unsigned long cur = millis();


// WiFi credentials
const char* WIFI_SSID = "<wifiname>";
const char* WIFI_PASSWORD = "<wifipassword>";

// MQTT configuration
const char* MQTT_SERVER = "<IP_Address>"; // GCP VM public IP
const char* MQTT_TOPIC = "dry-rack";   // Topic for the sensors
const int MQTT_PORT = 1883;

// Used Pins
const int btnPin1 = 14;
const int btnPin2 = 33;
const int dht11Pin = 21;
const int rainPin = 35;
const int servoPin = 16;
const int relayPin = 32;

// Sensor initialization
DHT dht(dht11Pin, DHTTYPE);
Servo servo;

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Last message time
unsigned long lastMsgTime = 0;

// Function to set up WiFi connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Reconnect to MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT server");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

// Smoothly move the servo to a target position
void moveServoSmooth(int targetAngle, int speed) {
  if (lastServoPosition < targetAngle) {
    for (int pos = lastServoPosition; pos <= targetAngle; pos++) {
      servo.write(pos);
      delay(speed);
    }
  } else if (lastServoPosition > targetAngle) {
    for (int pos = lastServoPosition; pos >= targetAngle; pos--) {
      servo.write(pos);
      delay(speed);
    }
  }
  lastServoPosition = targetAngle;  // Update the last position
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);

  pinMode(btnPin1, INPUT_PULLUP);
  pinMode(btnPin2, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);

  digitalWrite(relayPin, false);

  // Initialize sensors and actuators
  dht.begin();
  pinMode(rainPin, INPUT);
  servo.attach(servoPin, 500, 2500);
  servo.setPeriodHertz(50);
  servo.write(0);  // Start at the neutral position (rack is open)
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int button1 = !digitalRead(btnPin1); // Read button 1 value
  int button2 = !digitalRead(btnPin2); // Read button 2 value

  cur = millis();
  if (cur - lastMsgTime > 5000) {  // Send data every 5 seconds
    lastMsgTime = cur;

    // Publish combined telemetry data
    float h = dht.readHumidity();
    int t = dht.readTemperature();
    int raining = !digitalRead(rainPin);

    char Payload[256]; // Buffer for messages
    snprintf(Payload, sizeof(Payload), 
            "{\"Temperature\": %d, \"Humidity\": %.2f, \"Raining\": %d}", 
            t, h, raining);

    client.publish(MQTT_TOPIC, Payload); // Publish to a single topic
    Serial.println("Combined Data Published: ");
    Serial.println(Payload);


    // React to rain detection
    if (raining == 1 && isDrying == true && isFanOn == false) {
      isFanOn = true;
      digitalWrite(relayPin, true);
      Serial.println("Rain detected! Moving servo to retract position.");
      moveServoSmooth(0, servoSpeed);
    }

    // Button 1: Stop system
    if (button1) {
      isFanOn = false;
      isDrying = false;
      digitalWrite(relayPin, false);
      Serial.println("Stopping system...");
      if (lastServoPosition != 0) {
        moveServoSmooth(0, servoSpeed);
      }
    }

    // Button 2: Activate drying mode
    if (button2) {
      isDrying = true;
      if (lastServoPosition != 180) {
        moveServoSmooth(180, servoSpeed);
      }
    }
  }
}
