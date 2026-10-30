#define _DEBUG_

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ThingerESP32.h>
#include <DHT.h>
#include <UniversalTelegramBot.h>
#include <OneWire.h>             // ADDED: For DS18B20
#include <DallasTemperature.h>   // ADDED: For DS18B20

// =====================================================
// THINGER.IO CREDENTIALS
// =====================================================

#define USERNAME            "Rakuaza" //account thinger.io username
#define DEVICE_ID           "DCHM4"
#define DEVICE_CREDENTIAL   "dchm123"

// =====================================================
// WIFI CREDENTIALS
// =====================================================

#define SSID                "HONOR X7b 5G"  //change wifi name
#define SSID_PASSWORD       "p4e825kuf3eehif"       //change wifi password

// =====================================================
// TELEGRAM BOT CREDENTIALS
// =====================================================

#define BOT_TOKEN           "8935546631:AAGGXZLFmtd6LQcC2tXcaaXMIDAk-J6vYAo"
#define CHAT_ID             "351378392"
#define CHAT_ID             "-1004393536809"

// =====================================================
// SENSORS
// =====================================================

// DHT22 (Humidity Only)
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 (Temperature Only)
#define ONE_WIRE_BUS 5 // Connected to GPIO 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// =====================================================
// LED PINS
// =====================================================

#define LED_RED     23
#define LED_ORANGE  21
#define LED_YELLOW  19
#define LED_GREEN   18

// =====================================================
// THINGER.IO
// =====================================================

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

// =====================================================
// TELEGRAM
// =====================================================

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =====================================================
// VARIABLES
// =====================================================

float temperature = 0.0;
float humidity = 0.0;

String temperatureStatus = "UNKNOWN";
String previousStatus = "UNKNOWN";

// Changed to 0 to prevent "Fake Green" at startup
int statusCode = 0; 

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // Start Sensors
  dht.begin();
  sensors.begin(); // ADDED: Start DS18B20

  // Set LED pins as OUTPUT
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // Turn OFF all LEDs
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);

  // ===================================================
  // CONNECT TO WIFI
  // ===================================================

  thing.add_wifi(SSID, SSID_PASSWORD);

  // Allow Telegram HTTPS connection
  client.setInsecure();

  // ===================================================
  // THINGER.IO RESOURCES
  // ===================================================

  // Temperature
  thing["temperature"] >> [](pson &out) {
    out = temperature;
  };

  // Humidity
  thing["humidity"] >> [](pson &out) {
    out = humidity;
  };

  // Temperature status (Text)
  thing["status"] >> [](pson &out) {
    out = temperatureStatus;
  };

  // DCHM complete data
  thing["DCHM"] >> [](pson &out) {
    out["temperature"] = temperature;
    out["humidity"] = humidity;
    out["status"] = temperatureStatus;
    out["status_code"] = statusCode; 
  };

  // ===================================================
  // STARTUP MESSAGE
  // ===================================================

  Serial.println();
  Serial.println("======================================");
  Serial.println(" DCHM - Digital Community Heat Map");
  Serial.println("======================================");
  Serial.println("ESP32 is starting...");

  delay(2000);

  // Send startup notification to Telegram
  if (WiFi.status() == WL_CONNECTED) {

    String startupMessage;

    startupMessage = "DCHM System Started\n";
    startupMessage += "Digital Community Heat Map\n\n";
    startupMessage += "ESP32 is online.\n";
    startupMessage += "Monitoring temperature (DS18B20) and humidity (DHT22).";
    bot.sendMessage(CHAT_ID, startupMessage, "");
  }
}

// =====================================================
// SEND TELEGRAM ALERT
// =====================================================

void sendTelegramAlert() {

  String message;

  message = "DCHM Point 4 TEMPERATURE ALERT\n";
  message += "--------------------------\n";
  message += "Temperature: ";
  message += String(temperature, 1);
  message += " °C\n";

  message += "Related Humidity: ";
  message += String(humidity, 1);
  message += " %\n";

  message += "Status: ";
  message += temperatureStatus;
  message += "\n";

  message += "--------------------------\n";

  if (temperature < 27.0) {
    message += "Temperature level is LOW.\n";
    message += "Green LED is ON.";
  }
  else if (temperature >= 27.0 && temperature < 32.0) {
    message += "Temperature level is MODERATE.\n";
    message += "Yellow LED is ON.";
  }
  else if (temperature >= 32.0 && temperature <= 41.0) {
    message += "Temperature level is HIGH.\n";
    message += "Orange LED is ON.";
  }
  else {
    message += "WARNING: Temperature is VERY HIGH!\n";
    message += "Red LED is ON.";
  }

  bot.sendMessage(CHAT_ID, message, "");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // ===================================================
  // READ SENSORS
  // ===================================================
  
  // 1. Request and read temperature from DS18B20
  sensors.requestTemperatures(); 
  float newTemperature = sensors.getTempCByIndex(0);

  // 2. Read humidity from DHT22 (Ignore its temperature)
  float newHumidity = dht.readHumidity();

  // Check if either sensor failed (DS18B20 returns DEVICE_DISCONNECTED_C if it fails)
  if (newTemperature == DEVICE_DISCONNECTED_C || isnan(newHumidity)) {

    Serial.println("Failed to read from sensors! Check wiring.");

    thing.handle();

    delay(2000);

    return;
  }

  // Store valid sensor readings
  temperature = newTemperature;
  humidity = newHumidity;

  // ===================================================
  // TURN OFF ALL LEDs
  // ===================================================

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);

  // ===================================================
  // DETERMINE TEMPERATURE STATUS & ASSIGN STATUS CODE
  // ===================================================

  if (temperature < 27.0) {
    // Below 27°C
    digitalWrite(LED_GREEN, HIGH);
    temperatureStatus = "GREEN - Low Temperature";
    statusCode = 1; 
  }
  else if (temperature >= 27.0 && temperature < 32.0) {
    // 27°C to below 32°C
    digitalWrite(LED_YELLOW, HIGH);
    temperatureStatus = "YELLOW - Moderate Temperature";
    statusCode = 2; 
  }
  else if (temperature >= 32.0 && temperature <= 41.0) {
    // 32°C to 41°C
    digitalWrite(LED_ORANGE, HIGH);
    temperatureStatus = "ORANGE - High Temperature";
    statusCode = 3; 
  }
  else {
    // Above 41°C
    digitalWrite(LED_RED, HIGH);
    temperatureStatus = "RED - Very High Temperature";
    statusCode = 4; 
  }

  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.println("--------------------------------------");

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" °C (from DS18B20)");

  Serial.print("Relative Humidity    : ");
  Serial.print(humidity);
  Serial.println(" % (from DHT22)");

  Serial.print("DCHM Status : ");
  Serial.println(temperatureStatus);

  // ===================================================
  // TELEGRAM NOTIFICATION
  // ===================================================

  // Send Telegram message only when status changes
  if (temperatureStatus != previousStatus) {

    Serial.println("Temperature status changed!");
    Serial.println("Sending Telegram notification...");

    if (WiFi.status() == WL_CONNECTED) {

      sendTelegramAlert();

      Serial.println("Telegram notification sent.");
    }
    else {
      Serial.println("WiFi is not connected.");
      Serial.println("Telegram notification not sent.");
    }

    previousStatus = temperatureStatus;
  }

  // ===================================================
  // THINGER.IO

  // ===================================================

  thing.handle();

  // ===================================================
  // WAIT
  // ===================================================

  delay(2000);
}