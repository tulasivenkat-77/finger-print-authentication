#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

// Define the pins for the buttons
#define ENROLL_BUTTON_PIN 6 // Button to enroll a fingerprint
#define DELETE_BUTTON_PIN 7 // Button to delete a fingerprint

// Define the pin for the relay
#define RELAY_PIN A0 // Relay control pin (Analog Pin A0)

// GSM module RX and TX pins
#define GSM_RX_PIN 13
#define GSM_TX_PIN 10

// LCD Pin connections
#define RS 12
#define E 11
#define D4 5
#define D5 4
#define D6 3
#define D7 2

// Buzzer Pin
#define BUZZER_PIN A1  // Pin for the buzzer

// Initialize LCD
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);

// Create SoftwareSerial objects for GSM and fingerprint sensor
SoftwareSerial gsm(GSM_RX_PIN, GSM_TX_PIN);
SoftwareSerial mySerial(8, 9); // RX, TX pins for fingerprint sensor

// Initialize the fingerprint sensor object using the SoftwareSerial object
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Array to keep track of stored fingerprint IDs (1 = stored, 0 = empty)
bool storedIds[128] = {false}; // Index 0 not used, valid IDs are 1-127

// Variables to keep track of button states and modes
bool enrollMode = false;
bool deleteMode = false;
uint8_t nextEnrollID = 1; // Next available ID for enrollment

void setup() {
  Serial.begin(9600); // Initialize serial communication
  while (!Serial);    // Wait for serial to initialize
  delay(100);

  // Set up the LCD
  lcd.begin(16, 2);
  lcd.print(F("Initializing..."));

  // Initialize GSM module communication
  gsm.begin(9600);
  delay(3000); // Wait for GSM module to initialize
  gsm.println("AT");
  waitForGSMResponse("OK");

  // Set SMS mode to Text
  gsm.println("AT+CMGF=1");
  waitForGSMResponse("OK");

  Serial.println("GSM Module Initialized.");
  lcd.clear();
  lcd.print(F("GSM MODULE"));
  lcd.setCursor(0,1);
  lcd.print("Initialized");
  // Set up the fingerprint sensor
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println(F("Fingerprint sensor found!"));
    updateStoredIds(); // Initialize the stored IDs array
    displayFingerprintStatus(); // Display initial status
    lcd.clear();
    lcd.print(F("Place Finger"));
  } else {
    lcd.clear();
    lcd.print(F("Sensor not found"));
    Serial.println(F("Fingerprint sensor not found!"));
    while (1) {
      delay(1);
    }
  }

  // Initialize the button pins as inputs
  pinMode(ENROLL_BUTTON_PIN, INPUT);
  pinMode(DELETE_BUTTON_PIN, INPUT);

  // Set up the relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure the relay is initially off

  // Set up the buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure the buzzer is initially off
}

void loop() {
  // Continuously scan for fingerprints
  if (finger.getImage() == FINGERPRINT_OK) {
    int p = finger.image2Tz(1); // Convert image to template
    if (p != FINGERPRINT_OK) {
      lcd.clear();
      lcd.print(F("Failed to convert image"));
      Serial.println(F("Failed to convert image"));
      return;
    }

    // Try to identify the fingerprint using fast search
    p = finger.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
      lcd.clear();
      lcd.print(F("Fingerprint ID: "));
      lcd.setCursor(0,1);
      lcd.print(finger.fingerID);
      Serial.print(F("Fingerprint matched with ID: "));
      Serial.println(finger.fingerID);

      // Turn on the relay (motor on)
      digitalWrite(RELAY_PIN, HIGH);
    } else {
      lcd.clear();
      lcd.print(F("Fingerprint not "));
      lcd.setCursor(0,1);
      lcd.print("found");
      Serial.println(F("Fingerprint not matched!"));

      // Turn off the relay (motor off)
      digitalWrite(RELAY_PIN, LOW);

      // Sound the buzzer for 10 seconds with 1-second intervals
      soundBuzzer();

      // Display "Sending SMS..." on the LCD
      sendSMS("+918328648703", "Wrong authentication! Someone is trying to take your vehicle.");
    }
    delay(2000); // Show result for 2 seconds
  }

  // Check if the Enroll button is pressed
  if (digitalRead(ENROLL_BUTTON_PIN) == HIGH) {
    if (!enrollMode) {
      enrollMode = true;
      deleteMode = false;
      nextEnrollID = findNextAvailableID();
      lcd.clear();
      lcd.print(F("Enroll Mode"));
      lcd.setCursor(0, 1);
      lcd.print(F("ID: "));
      lcd.print(nextEnrollID);
      Serial.print(F("Entering Enroll Mode. Next ID: "));
      Serial.println(nextEnrollID);
      delay(1000);
    } else {
      enrollMode = false;
      enrollFingerprint(nextEnrollID); // Enroll a new fingerprint
    }
  }

  // Check if the Delete button is pressed
  if (digitalRead(DELETE_BUTTON_PIN) == HIGH) {
    if (!deleteMode) {
      deleteMode = true;
      enrollMode = false;
      lcd.clear();
      lcd.print(F("Delete Mode"));
      Serial.println(F("Entering Delete Mode"));
      delay(1000);
    } else {
      deleteMode = false;
      deleteLastFingerprint(); // Delete the last stored fingerprint
    }
  }
}

// Function to sound the buzzer for 10 seconds with 1 second delay between each second
void soundBuzzer() {
  for (int i = 0; i < 10; i++) {  // Loop for 10 seconds
    digitalWrite(BUZZER_PIN, HIGH);  // Turn buzzer on
    delay(1000);                      // Wait for 1 second
    digitalWrite(BUZZER_PIN, LOW);   // Turn buzzer off
    delay(1000);                      // Wait for 1 second
  }
}

// Function to find the next available ID
uint8_t findNextAvailableID() {
  for (int i = 1; i <= 127; i++) {
    if (!storedIds[i]) {
      return i;
    }
  }
  return 0; // No available ID
}

// Function to enroll a fingerprint
void enrollFingerprint(uint8_t id) {
  lcd.clear();
  lcd.print(F("Enrolling ID: "));
  lcd.print(id);
  Serial.print(F("Enrolling ID #"));
  Serial.println(id);

  while (!getFingerprintEnroll(id)) {
    delay(100);
  }

  displayFingerprintStatus();
}

uint8_t getFingerprintEnroll(uint8_t id) {
  int p = -1;
  lcd.clear();
  lcd.print(F("Place finger"));
  Serial.println(F("Waiting for valid finger"));

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println(F("Image taken"));
        lcd.clear();
        lcd.print(F("Image taken"));
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(F("No finger detected"));
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println(F("Communication error"));
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println(F("Imaging error"));
        break;
      default:
        Serial.println(F("Unknown error"));
        break;
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println(F("Image conversion failed"));
    return p;
  }

  lcd.clear();
  lcd.print(F("Remove finger"));
  Serial.println(F("Remove finger"));
  delay(2000);
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  lcd.clear();
  lcd.print(F("Place again"));
  Serial.println(F("Place same finger again"));
  
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println(F("Image conversion failed"));
    return p;
  }

  Serial.println(F("Creating model..."));

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println(F("Failed to create model"));
    return p;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    storedIds[id] = true;
    Serial.println(F("Stored!"));
    lcd.clear();
    lcd.print(F("Stored ID: "));
    lcd.print(id);
    delay(2000);
    displayFingerprintStatus();
  } else {
    Serial.println(F("Error storing fingerprint"));
    return p;
  }
  
  return true;
}

// Function to delete the last stored fingerprint
void deleteLastFingerprint() {
  lcd.clear();
  lcd.print(F("Deleting Finger"));
  Serial.println(F("Ready to delete a fingerprint!"));

  // Find the last stored ID
  uint8_t id = 0;
  for (int i = 127; i >= 1; i--) {
    if (storedIds[i]) {
      id = i;
      break;
    }
  }

  if (id == 0) { // No stored IDs
    lcd.clear();
    lcd.print(F("No IDs to delete"));
    return;
  }

  Serial.print(F("Deleting ID #"));
  Serial.println(id);

  int p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK) {
    storedIds[id] = false;
    Serial.println(F("Deleted!"));
    lcd.clear();
    lcd.print(F("Deleted ID: "));
    lcd.print(id);
    delay(2000);
    displayFingerprintStatus();
    nextEnrollID = findNextAvailableID(); // Update the next enroll ID
  } else {
    Serial.println(F("Error deleting fingerprint"));
    lcd.clear();
    lcd.print(F("Delete failed"));
    delay(1000);
  }
}

// Function to update stored IDs array and count
void updateStoredIds() {
  for (int i = 1; i <= 127; i++) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      storedIds[i] = true;
    } else {
      storedIds[i] = false;
    }
  }
}

// Function to display fingerprint storage status
void displayFingerprintStatus() {
  Serial.println(F("\n=== Fingerprint Storage Status ==="));
  updateStoredIds(); // Update the stored IDs array and count
  uint8_t fingerprintCount = 0;
  for (int i = 1; i <= 127; i++) {
    if (storedIds[i]) {
      fingerprintCount++;
    }
  }
  Serial.print(F("Total stored fingerprints: "));
  Serial.println(fingerprintCount);
  lcd.clear();
  lcd.print(F("Stored FPs: "));
  lcd.print(fingerprintCount);
  if (fingerprintCount > 0) {
    Serial.println(F("\nStored fingerprint IDs:"));
    lcd.setCursor(0, 1); // Display stored IDs
    for (int i = 1; i <= 127; i++) {
      if (storedIds[i]) {
        Serial.print(F("ID #"));
        Serial.println(i);
        lcd.clear();
        lcd.print(F("Stored FPs: "));
        lcd.print(fingerprintCount);
        lcd.setCursor(0, 1);
        lcd.print(F("ID: "));
        lcd.print(i);
        delay(1000);
      }
    }
  } else {
    Serial.println(F("No fingerprints stored"));
    lcd.setCursor(0, 1);
    lcd.print(F("No stored FPs"));
  }
  Serial.println(F("==============================\n"));
  delay(2000);
}

// Function to send an SMS
void sendSMS(String phoneNumber, String message) {
  Serial.println("Sending SMS...");

  // Display "Sending SMS..." on the LCD
  lcd.clear();
  lcd.print(F("Sending SMS..."));
  delay(1000);  // Wait a moment to show the message

  // Send the AT command to start SMS
  gsm.print("AT+CMGS=\"");
  gsm.print(phoneNumber);
  gsm.println("\"");
  delay(100); // Wait for the GSM module to respond

  // Send the message text
  gsm.print(message);
  delay(100); // Wait for the GSM module to respond

  // End the message with Ctrl+Z (ASCII 26)
  gsm.write(26);
  delay(5000); // Wait for the SMS to be sent

  if (gsm.available()) {
    String response = gsm.readString();
    Serial.println("GSM Response: ");
    Serial.println(response);
  }

  // Display "Message Sent" on the LCD
  lcd.clear();
  lcd.print(F("Message Sent"));
  delay(2000);  // Wait for 2 seconds to show the message
}

// Function to wait for a specific response from GSM module
void waitForGSMResponse(String expectedResponse) {
  String response = "";
  long timeout = millis() + 5000; // 5 seconds timeout

  while (millis() < timeout) {
    if (gsm.available()) {
      char c = gsm.read();
      response += c;
    }

    if (response.indexOf(expectedResponse) != -1) {
      Serial.println("Response: " + response);
      return;
    }
  }

  Serial.println("Response timeout or unexpected response: " + response);
}

// Function to read an ID number from the Serial input
uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available());  // Wait for input
    num = Serial.parseInt();  // Parse the input number
  }
  return num;
}
