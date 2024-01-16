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
bool rfid_status = true;
int parking_available = 0;
int parking_total = 0;
String plate_number = "";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("SYSTEM INIT...");

  // TFT init
  tft.init();
  tft.setRotation(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(tft.width());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.fillScreen(TFT_BLACK);

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
  tft.drawString("READY TO SCAN", tft.width()/2, 60);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("RFID sticker detected!");

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

    // To get parking data
    Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathParking.c_str());
    content.setJsonData(fbdo.payload().c_str());
    FirebaseJsonData jsonParkingData1;
    FirebaseJsonData jsonParkingData2;
    content.get(jsonParkingData1, "fields/parking_available/integerValue", true);
    content.get(jsonParkingData2, "fields/parking_total/integerValue", true);
    parking_available = jsonParkingData1.intValue;
    parking_total = jsonParkingData2.intValue;

    // Your additional logic here
    // Check the RFID Tag ID exist in Firestore or not
    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathRfid.c_str())) {
      Serial.println("RFID tag exist!");

      content.clear();
      content.setJsonData(fbdo.payload().c_str());

      FirebaseJsonData jsonData1;
      FirebaseJsonData jsonData2;
      FirebaseJsonData jsonData3;
      content.get(jsonData1, "fields/parking_entered/booleanValue", true);
      content.get(jsonData2, "fields/rfid_status/booleanValue", true);
      content.get(jsonData3, "fields/plate_number/stringValue", true);
      parking_entered = jsonData1.boolValue;
      rfid_status = jsonData2.boolValue;
      plate_number = jsonData3.stringValue;

      if (parking_entered == false) {
        if (rfid_status == true) {
          if (parking_available <= parking_total) {
            parking_available++;
            parking_entered = true;

            content.clear();
            content.set("fields/parking_available/integerValue", String(parking_available).c_str());
            content.set("fields/parking_entered/booleanValue", String(parking_entered).c_str());
            Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathParking.c_str(), content.raw(), "parking_available");
            Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPathRfid.c_str(), content.raw(), "parking_entered");

            Serial.println("Gate opened to enter");

            tft.setTextColor(TFT_BLACK);
            tft.fillScreen(TFT_GREEN);
            tft.drawString(plate_number, tft.width()/2, 50);
            tft.drawString("GATE OPENED ENTER", tft.width()/2, 80);
            
            servo1.write(90);
            delay(6000);
          } else {
            Serial.println("Parking is full");

            tft.setTextColor(TFT_WHITE);
            tft.fillScreen(TFT_RED);
            tft.drawString("PARKING FULL", tft.width()/2, 60);

            delay(5000);
          }
        } else {
          Serial.println("RFID tag terminated/expired!");

          tft.setTextColor(TFT_WHITE);
          tft.fillScreen(TFT_RED);
          tft.drawString("RFID TAG TERMINATED", tft.width()/2, 60);

          delay(5000);
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

        tft.setTextColor(TFT_BLACK);
        tft.fillScreen(TFT_GREEN);
        tft.drawString(plate_number, tft.width()/2, 50);
        tft.drawString("GATE OPENED EXIT", tft.width()/2, 80);

        servo2.write(90);
        delay(6000);
      }
    } else {
      Serial.println("RFID tag not exist!");

      tft.setTextColor(TFT_WHITE);
      tft.fillScreen(TFT_RED);
      tft.drawString("RFID TAG INVALID", tft.width()/2, 60);

      delay(5000);
    }

    Serial.println("Gate closed");

    tft.setTextColor(TFT_WHITE);
    tft.fillScreen(TFT_RED);
    tft.drawString("GATE CLOSED", tft.width()/2, 60);

    servo1.write(190);
    servo2.write(0);
    Serial.println("===============================");

    delay(3000);

    tft.setTextColor(TFT_WHITE);
    tft.fillScreen(TFT_BLACK);
    String totalAvailParking = String(parking_total - parking_available);
    tft.drawString("UNIPARK@UITM", tft.width()/2, 50);
    tft.drawString("PARKING AVAIL: " + totalAvailParking, tft.width()/2, 80);

    delay(1000); // Add a delay to prevent multiple readings in a short time
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
