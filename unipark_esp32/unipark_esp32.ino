#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h> // Provide the token generation process info.

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define WEB_API_KEY ""
#define FIREBASE_PROJECT_ID ""

#define USER_EMAIL ""
#define USER_PASSWORD ""

#define SS_PIN 33  // SDA pin for RFID-RC522
#define RST_PIN 17 // RST pin for RFID-RC522

// MFRC522 Variables
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ESP32Servo Variables
Servo servo1;
Servo servo2;

// TFT Variables
TFT_eSPI tft = TFT_eSPI();

// Firebase Variables
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long dataMillis = 0;

// Variables
bool parking_entered = false;
int parking_available = 0;
int parking_total = 13;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("SYSTEM INIT...");

  // TFT init
  tft.init();
  tft.fillScreen(TFT_WHITE);

  // Servo init
  servo1.attach(12);
  servo2.attach(13);
  servo1.write(190);
  servo2.write(0);

  // WiFi init
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Firebase init
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = WEB_API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectNetwork(true);

  fbdo.setBSSLBufferSize(4096, 1024);

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  // MFRC522 init
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  Serial.println(digitalRead(RST_PIN));

  SPI.begin(25, 27, 26); // Init SPI bus CLK, MISO, MOSI
  mfrc522.PCD_Init(); // Init MFRC522
  delay(1); // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
  Serial.println("READY TO SCAN");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("RFID stciker detected!");

    // Read RFID card UID and convert it to a String
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      // Convert each byte to a string and remove the leading zero, if any
      uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
    }

    // Convert the uidString to uppercase
    uidString.toUpperCase();

    Serial.print("UID Value: ");
    Serial.println(uidString);

    FirebaseJson content;

    String documentPathRfid = "RFID/" + uidString;
    String documentPathParking = "Parking/NxKl9i1D5RMUeQDveWF7";

    // Your additional logic here
    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathRfid.c_str(), content.raw())) {
      Serial.println("RFID tag exist!");

      if (parking_entered == false) {
        if (parking_available <= parking_total) {
          parking_available++;
          parking_entered = true;
          content.clear();
          content.set("fields/parking_available/integerValue", String(parking_available).c_str());
          content.set("fields/parking_entered/booleanValue", String(parking_entered).c_str());
          Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathParking.c_str(), content.raw(), "parking_available");
          Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathRfid.c_str(), content.raw(), "parking_entered");

          Serial.println("Gate opened to enter");
          tft.fillScreen(TFT_GREEN);
          servo1.write(90);
          delay(5000);
        } else {
          Serial.println("Parking is full");
          tft.fillScreen(TFT_RED);
        }
      } else {
        parking_available--;
        parking_entered = false;
        content.clear();
        content.set("fields/parking_available/integerValue", String(parking_available).c_str());
        content.set("fields/parking_entered/booleanValue", String(parking_entered).c_str());
        Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathParking.c_str(), content.raw(), "parking_available");
        Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathRfid.c_str(), content.raw(), "parking_entered");

        Serial.println("Gate opened to exit");
        tft.fillScreen(TFT_GREEN);
        servo2.write(90);
        delay(5000);
      }
    } else {
      Serial.println("RFID tag not exist!");
      tft.fillScreen(TFT_RED);
      delay(5000);
    }

    tft.fillScreen(TFT_WHITE);
    Serial.println("Gate closed");
    servo1.write(190);
    servo2.write(0);
    Serial.println("===============================");
    delay(1000);  // Add a delay to prevent multiple readings in a short time
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
