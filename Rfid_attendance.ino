#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

// ---------------- RFID Setup ----------------
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------------- RTC Setup ----------------
RTC_DS3231 rtc;

// ---------------- GSM Setup ----------------
SoftwareSerial gsm(2, 3); // RX, TX

// ---------------- LCD Setup ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------------- Feedback ----------------
#define BUZZER 6
#define LED 7

// ---------------- Hardcoded Students ----------------
struct Student {
  String uidIn;       // Card used for Entry
  String uidOut;      // Card used for Exit
  String name;
  String gradeSection;
  String parentNumber;
};

// Replace with your actual IDs and numbers
Student students[] = {
  {"CA1A7F5", "97AF1F15", "Juan Dela Cruz", "Grade 7-AI", "+639661971052"},
  {"69A05E12", "51C9BB97", "Maria Santos",   "Grade 7-AI", "+639661971052"}
};

int studentCount = sizeof(students)/sizeof(students[0]);

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
    // Serial.println("RTC lost power, let's set the time!");
    
    // This line sets the RTC to the date & time this sketch was compiled
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  

  gsm.begin(9600);
  lcd.init();
  lcd.backlight();

  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  lcd.setCursor(0,0);
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
  lcd.print("Ready for Scan");
}

// ---------------- Main Loop ----------------
void loop() {
  // Check if a new RFID card is present
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // Read UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  // Variables
  String studentName = "";
  String parentNumber = "";
  String gradeSection = "";
  String attendanceType = ""; // "IN" or "OUT"
  
  // Search student
  for (int i=0; i < studentCount; i++) {
    if (uid == students[i].uidIn) {
      studentName = students[i].name;
      parentNumber = students[i].parentNumber;
      gradeSection = students[i].gradeSection;
      attendanceType = "IN";
      break;
    }
    else if (uid == students[i].uidOut) {
      studentName = students[i].name;
      parentNumber = students[i].parentNumber;
      gradeSection = students[i].gradeSection;
      attendanceType = "OUT";
      break;
    }
  }

  if (studentName != "") {
    
    // --- 1. Get Time ---
    DateTime now = rtc.now();
    
    int hour24 = now.hour(); // 0-23 format
    int minute = now.minute();

    // Format for display (12-hour format)
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12; 
    String period = (hour24 < 12) ? " AM" : " PM";
    String minuteStr = (minute < 10) ? "0" + String(minute) : String(minute);
    String timeOnly = String(hour12) + ":" + minuteStr + period;
    String timestamp = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day());

    // --- 2. Determine LATE vs ON-TIME ---
    String remarks = "";
    String shift = ""; // "Morning" or "Afternoon"

    if (attendanceType == "IN") {
      
      // MORNING SHIFT (Before 12:00 PM)
      if (hour24 < 12) {
        shift = "AM";
        // Cutoff: 8:00 AM
        if (hour24 > 8 || (hour24 == 8 && minute > 0)) {
          remarks = "LATE";
        } else {
          remarks = "ON-TIME";
        }
      } 
      // AFTERNOON SHIFT (12:00 PM and onwards)
      else {
        shift = "PM";
        // Cutoff: 1:00 PM (13:00)
        if (hour24 > 13 || (hour24 == 13 && minute > 0)) {
          remarks = "LATE";
        } else {
          remarks = "ON-TIME";
        }
      }

    } else {
      // Logic for OUT scans
      remarks = "DISMISSAL"; 
    }

    // --- 3. LCD Feedback ---
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(studentName);
    lcd.setCursor(0,1);
    
    if(attendanceType == "IN") {
      // Example: "IN-PM LATE" or "IN-AM ON-TIME"
      lcd.print(shift + " " + remarks); 
    } else {
      lcd.print("OUT - Goodbye");
    }
    
    tone(BUZZER, 2700);
    digitalWrite(LED, HIGH);
    delay(500);
    noTone(BUZZER);
    digitalWrite(LED, LOW);

    // --- 4. Serial Output ---
    Serial.println(uid + "," + studentName + "," + gradeSection + "," + attendanceType + "," + remarks + "," + timestamp + "," + timeOnly);

    delay(1500); 
    lcd.clear();
    lcd.print("Ready for Scan");

  } else {
    // Unknown Card
    lcd.clear();
    lcd.print("Unknown Card");
    lcd.setCursor(0,1);
    lcd.print(uid);
    tone(BUZZER, 200); 
    delay(500);
    noTone(BUZZER);
    delay(1000);
    lcd.clear();
    lcd.print("Ready for Scan");
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
